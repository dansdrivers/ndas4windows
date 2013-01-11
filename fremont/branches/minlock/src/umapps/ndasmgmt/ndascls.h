#pragma once

#include <ndas/ndasuser.h>
#include <ndas/ndasmsg.h>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

namespace ndas {

	class CUpdatableObject
	{
		DWORD m_dwMinInterval;
		DWORD m_dwLastUpdated;

	protected:

		CUpdatableObject(DWORD dwMinInterval = 1500) : 
			m_dwMinInterval(dwMinInterval),
			m_dwLastUpdated(0)
		{}

		void OnUpdate() 
		{ m_dwLastUpdated = ::GetTickCount(); }

		BOOL IsExpired() 
		{ return ((::GetTickCount() - m_dwLastUpdated) > m_dwMinInterval); }

	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////
	
	class Device;
	class UnitDevice;
	class LogicalDevice;

	typedef boost::shared_ptr<UnitDevice> UnitDevicePtr;
	typedef boost::shared_ptr<Device> DevicePtr;
	typedef boost::shared_ptr<LogicalDevice> LogicalDevicePtr;

	typedef boost::weak_ptr<UnitDevice> UnitDeviceWeakPtr;
	typedef boost::weak_ptr<Device> DeviceWeakPtr;

	typedef std::vector<DevicePtr> DeviceVector;
	typedef std::vector<UnitDevicePtr> UnitDeviceVector;
	typedef std::vector<LogicalDevicePtr> LogicalDeviceVector;

	typedef DeviceVector::const_iterator DeviceConstIterator;
	typedef DeviceVector::iterator DeviceIterator;

	typedef UnitDeviceVector::const_iterator UnitDeviceConstIterator;
	typedef UnitDeviceVector::iterator UnitDeviceIterator;

	typedef LogicalDeviceVector::const_iterator LogicalDeviceConstIterator;
	typedef LogicalDeviceVector::iterator LogicalDeviceIterator;

	typedef boost::shared_ptr<NDAS_DEVICE_HARDWARE_INFO> NdasDeviceHardwareInfoPtr;
	typedef boost::shared_ptr<const NDAS_DEVICE_HARDWARE_INFO> NdasDeviceHardwareInfoConstPtr;

	//
	// Global Function to hold device and logical device pointers
	// as singletons
	//
	const DeviceVector& GetDevices();
	const LogicalDeviceVector& GetLogicalDevices();

	class Device :
		public CUpdatableObject
	{
		const DWORD m_slot;
		NDAS_DEVICE_STATUS m_status;
		NDAS_DEVICE_ERROR m_lastError;

		TCHAR m_id[NDAS_DEVICE_STRING_ID_LEN + 1];
		TCHAR m_name[MAX_NDAS_DEVICE_NAME_LEN + 1];
		ACCESS_MASK m_grantedAccess;
		UnitDeviceVector m_unitDevices;

		NdasDeviceHardwareInfoPtr m_pHardwareInfo;
		NDAS_DEVICE_STAT m_dstats;

		DWORD m_lastUpdateTick;

	public:

		Device(
			DWORD slot, 
			LPCTSTR szId,
			LPCTSTR szName, 
			ACCESS_MASK grantedAccess);

		DWORD GetSlotNo();
		ACCESS_MASK GetGrantedAccess();
		BOOL SetAsReadOnly();
		BOOL SetAsReadWrite(LPCTSTR szKey);
		LPCTSTR GetName();
		BOOL SetName(LPCTSTR szName);
		NDAS_DEVICE_STATUS GetStatus();
		NDAS_DEVICE_ERROR GetLastError();
		LPCTSTR GetStringId();

		NdasDeviceHardwareInfoConstPtr GetHardwareInfo();

		BOOL Enable(BOOL bEnable = TRUE);
		BOOL UpdateInfo();
		BOOL UpdateStatus();
		BOOL UpdateStats();

		const UnitDeviceVector& GetUnitDevices()
		{
			return m_unitDevices;
		}
		
		bool FindUnitDevice(UnitDevicePtr& p, DWORD UnitNo);
		bool IsAnyUnitDeviceMounted();


	protected:

		static BOOL CALLBACK 
		EnumUnitDeviceProc(
			PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry, 
			LPVOID lpContext)
		{
			Device* pDevice = static_cast<Device*>(lpContext);
			return pDevice->OnEnumUnitDevice(lpEntry);
		}

		BOOL OnEnumUnitDevice(PNDASUSER_UNITDEVICE_ENUM_ENTRY Entry);
	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Unit Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class UnitDevice :
		public CUpdatableObject
	{
		boost::weak_ptr<Device> m_pDevice;
		const DWORD m_unitNo;
		NDAS_UNITDEVICE_STATUS m_status;
		NDAS_UNITDEVICE_ERROR m_lastError;
		NDAS_UNITDEVICE_TYPE m_type;
		NDAS_UNITDEVICE_SUBTYPE m_subtype;
		NDAS_UNITDEVICE_HARDWARE_INFO* m_pHWInfo;

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

		BOOL OnEnumHost(LPCGUID lpHostGuid, ACCESS_MASK access);

		static BOOL CALLBACK EnumHostProc(LPCGUID lpHostGuid, ACCESS_MASK access, LPVOID lpInstance)
		{
			UnitDevice* pThis = static_cast<UnitDevice*>(lpInstance);
			return pThis->OnEnumHost(lpHostGuid, access);
		}

	public:
		UnitDevice(
			DevicePtr pDevice,
			DWORD unitNo, 
			NDAS_UNITDEVICE_TYPE type);
		~UnitDevice();

		DeviceWeakPtr GetParentDevice();
		DWORD GetSlotNo();
		DWORD GetUnitNo();
		const NDAS_UNITDEVICE_HARDWARE_INFO* GetHWInfo();
		NDAS_UNITDEVICE_TYPE GetType();
		NDAS_UNITDEVICE_SUBTYPE GetSubType();
		NDAS_UNITDEVICE_STATUS GetStatus();
		NDAS_UNITDEVICE_ERROR GetLastError();

		NDAS_LOGICALDEVICE_ID GetLogicalDeviceId();

		BOOL UpdateHostInfo();
		DWORD GetHostInfoCount();
		const NDAS_HOST_INFO* GetHostInfo(
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
	// NDAS Logical Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class LogicalDevice :
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

		const NDAS_LOGICALDEVICE_ID m_id;
		const NDAS_LOGICALDEVICE_TYPE m_type;

		NDAS_LOGICALDEVICE_STATUS m_status;
		NDAS_LOGICALDEVICE_ERROR m_lastError;

		typedef std::vector<UNITDEVICE_INFO> UnitDeviceInfoVector;
		UnitDeviceInfoVector m_unitDevices;

	protected:

		static BOOL CALLBACK 
		EnumMemberProc(
			PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY lpEntry, 
			LPVOID lpContext)
		{
			LogicalDevice* pThis = static_cast<LogicalDevice*>(lpContext);
			return pThis->OnEnumMember(lpEntry);
		}

		BOOL OnEnumMember(PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY lpEntry);

		NDASUSER_LOGICALDEVICE_INFORMATION m_logDeviceInfo;

	public:

		LogicalDevice(
			NDAS_LOGICALDEVICE_ID logicalDeviceId,
			NDAS_LOGICALDEVICE_TYPE type);

		NDAS_LOGICALDEVICE_ID GetLogicalDeviceId();

		ACCESS_MASK GetGrantedAccess();
		ACCESS_MASK GetMountedAccess();

		DWORD GetLogicalDrives();

		NDAS_LOGICALDEVICE_STATUS GetStatus();
		NDAS_LOGICALDEVICE_ERROR GetLastError();

		NDAS_LOGICALDEVICE_TYPE GetType();

		const UNITDEVICE_INFO& GetUnitDeviceInfo(DWORD dwIndex);
		DWORD GetUnitDeviceInfoCount();

		BOOL UpdateInfo();
		BOOL UpdateStatus();

		BOOL PlugIn(BOOL bReadWrite = false);
		BOOL Eject();
		BOOL Eject(NDAS_LOGICALDEVICE_EJECT_PARAM* EjectParam);
		BOOL Unplug();

		const NDASUSER_LOGICALDEVICE_INFORMATION*
		GetLogicalDeviceInfo();

		BOOL IsContentEncrypted();

		CString GetName();
	};

	bool FindDeviceBySlotNumber(DevicePtr& p, DWORD SlotNumber);
	bool FindDeviceByNdasId(DevicePtr& p, LPCTSTR szNdasId);
	bool FindDeviceByName(DevicePtr& p, LPCTSTR Name);
	bool FindLogicalDevice(LogicalDevicePtr& p, NDAS_LOGICALDEVICE_ID Id);

	bool UpdateDeviceList();
	bool UpdateLogicalDeviceList();

	void GetMountedLogicalDevices(LogicalDeviceVector& dest);


struct DeviceSlotNumberEquals : std::unary_function<DevicePtr,bool>
{
	DeviceSlotNumberEquals(DWORD slotNumber) : SlotNumber(slotNumber) {}
	bool operator()(DevicePtr p) const
	{
		bool v = (p->GetSlotNo() == SlotNumber);
		return v;
	}
private:
	DWORD SlotNumber;
};

struct DeviceNameEquals : std::unary_function<DevicePtr,bool>
{
	DeviceNameEquals(LPCTSTR name) : DeviceName(name) {}
	bool operator()(DevicePtr p) const
	{
		bool v = (0 == ::lstrcmpi(p->GetName(), DeviceName));
		return v;
	}
private:
	LPCTSTR DeviceName;
};

struct DeviceIdEquals : std::unary_function<DevicePtr,bool>
{
	DeviceIdEquals(LPCTSTR id) : DeviceId(id) {}
	bool operator()(DevicePtr p) const
	{
		bool v = (0 == ::lstrcmpi(p->GetStringId(), DeviceId));
		return v;
	}
private:
	LPCTSTR DeviceId;
};

struct LogicalDeviceIdEquals : std::unary_function<LogicalDevicePtr,bool>
{
	LogicalDeviceIdEquals(NDAS_LOGICALDEVICE_ID id) : Id(id) {}
	bool operator()(LogicalDevicePtr p) const
	{
		bool v = (p->GetLogicalDeviceId() == Id);
		return v;
	}
private:
	NDAS_LOGICALDEVICE_ID Id;
};

struct UnitDeviceNumberEqual : 
	std::unary_function<UnitDevicePtr, bool>
{
	UnitDeviceNumberEqual(DWORD n) : UnitNo(n) {}
	bool operator()(UnitDevicePtr p) const
	{
		bool v = (p->GetUnitNo() == UnitNo);
		return v;
	}
private:
	DWORD UnitNo;
};

struct UnitDeviceMounted : std::unary_function<UnitDevicePtr,bool>
{
	bool operator()(UnitDevicePtr p) const
	{
		bool v = (NDAS_UNITDEVICE_STATUS_MOUNTED == p->GetStatus());
		return v;
	}
};

struct LogicalDeviceMounted : std::unary_function<LogicalDevicePtr,bool>
{
	bool operator()(LogicalDevicePtr p) const
	{
		bool v = (NDAS_LOGICALDEVICE_STATUS_MOUNTED == p->GetStatus());
		return v;
	}
};

struct DeviceNameSortAscending : std::binary_function<DevicePtr, DevicePtr, bool>
{
	bool operator()(DevicePtr p1, DevicePtr p2) const
	{
		bool v = ::lstrcmpi(p1->GetName(), p2->GetName()) < 0;
		return v;
	}
};

}
