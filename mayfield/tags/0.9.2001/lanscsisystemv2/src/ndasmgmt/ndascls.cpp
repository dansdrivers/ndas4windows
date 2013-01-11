#include "stdafx.h"
#include "ndascls.h"

namespace ndas {

#ifndef NO_NDAS_API_EXCEPTION
	///////////////////////////////////////////////////////////////////////
	// APIException class
	///////////////////////////////////////////////////////////////////////
	
	APIException::APIException(DWORD dwErrorCode) :
		m_szMessage(0),
		m_dwErrorCode(dwErrorCode)
	{
	}

	APIException::~APIException()
	{
		if (0 != m_szMessage) {
			::LocalFree(reinterpret_cast<HLOCAL>(m_szMessage));
		}
	}

	DWORD APIException::GetErrorCode()
	{
		return m_dwErrorCode;
	}

	bool APIException::IsSystemError()
	{
		return !(m_dwErrorCode & APPLICATION_ERROR_MASK);
	}

	LPCTSTR APIException::ToString()
	{
		if (0 != m_szMessage) {
			return m_szMessage;
		}

		LPVOID lpSource(NULL);
		DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS;

		if (IsSystemError()) {
			dwFlags |= FORMAT_MESSAGE_FROM_SYSTEM;
		} else {
			dwFlags |= FORMAT_MESSAGE_FROM_HMODULE;
			HMODULE hModule = ::LoadLibraryEx(
				_T("ndasmsg.dll"),
				NULL,
				LOAD_LIBRARY_AS_DATAFILE);
			lpSource = hModule;
			if (NULL == hModule) {
				return _T("");
			}

		}

		BOOL fSuccess = ::FormatMessage(
			dwFlags,
			lpSource,
			m_dwErrorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &m_szMessage,
			0,
			NULL);

		if (!fSuccess) {
			return _T("");
		}

		if (NULL != lpSource) {
			::FreeLibrary((HMODULE)lpSource);
		}

		return m_szMessage;
	}

#endif // NO_NDAS_API_EXCEPTION

	///////////////////////////////////////////////////////////////////////
	// Device Collection class
	///////////////////////////////////////////////////////////////////////
	DeviceColl::~DeviceColl()
	{
		Clear();
	}

	void DeviceColl::Clear()
	{
		DeviceVector::iterator itr = m_devices.begin();
		while (itr != m_devices.end()) {
			Device* pDevice = *itr;
			pDevice->Release();
			itr = m_devices.erase(itr);
		}
		_ASSERTE(m_devices.size() == 0);
	}

	BOOL DeviceColl::Update()
	{
		Clear();
		return ::NdasEnumDevicesW(
			DeviceColl::EnumProc, 
			reinterpret_cast<LPVOID>(this));
	};

	BOOL CALLBACK 
	DeviceColl::EnumProc(
		PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, 
		LPVOID lpContext)
	{
		DeviceColl* pDeviceColl = 
			reinterpret_cast<DeviceColl*>(lpContext);

		Device* pDevice = new Device(
			lpEnumEntry->SlotNo,
			lpEnumEntry->szDeviceStringId,
			lpEnumEntry->szDeviceName,
			lpEnumEntry->GrantedAccess);

		pDevice->AddRef();
		pDeviceColl->m_devices.push_back(pDevice);

		return TRUE;
	}

	Device* DeviceColl::FindDevice(DWORD slot)
	{
		DeviceVector::const_iterator itr = m_devices.begin();
		while (itr != m_devices.end()) {
			Device* pDevice = *itr;
			if (slot == pDevice->GetSlotNo()) {
				pDevice->AddRef();
				return pDevice;
			}
			++itr;
		}
		return NULL;
	}

	Device* DeviceColl::FindDevice(LPCTSTR szStringId)
	{
		DeviceVector::const_iterator itr = m_devices.begin();
		while (itr != m_devices.end()) {
			Device* pDevice = *itr;
			if (0 == lstrcmpi(pDevice->GetStringId(), szStringId)) {
				pDevice->AddRef();
				return pDevice;
			}
			++itr;
		}
		return NULL;
	}

	Device* DeviceColl::GetDevice(DWORD index)
	{
		if (index >= m_devices.size()) {
			return NULL;
		}
		m_devices[index]->AddRef();
		return m_devices[index];
	}

	DWORD DeviceColl::GetDeviceCount()
	{
		return (DWORD) m_devices.size();
	}

	BOOL DeviceColl::Register()
	{
		return FALSE;
	}

	BOOL DeviceColl::Unregister(DWORD slotNo)
	{
		BOOL fSuccess = ::NdasUnregisterDevice(slotNo);
		return fSuccess;
	}

	///////////////////////////////////////////////////////////////////////
	// Device class
	///////////////////////////////////////////////////////////////////////

	Device::Device(
		DWORD slot, 
		LPCTSTR szId,
		LPCTSTR szName, 
		ACCESS_MASK grantedAccess) :
		m_slot(slot),
		m_hwinfo(NULL),
		m_status(NDAS_DEVICE_STATUS_UNKNOWN),
		m_lastError(NDAS_DEVICE_ERROR_NONE)
	{
		::ZeroMemory(
			m_name, 
			MAX_NDAS_DEVICE_NAME_LEN + 1);

		HRESULT hr = ::StringCchCopy(
			m_name, 
			MAX_NDAS_DEVICE_NAME_LEN + 1, 
			szName);

		_ASSERTE(SUCCEEDED(hr));

		::ZeroMemory(
			m_id,
			NDAS_DEVICE_STRING_ID_LEN + 1);

		hr = ::StringCchCopy(
			m_id, 
			NDAS_DEVICE_STRING_ID_LEN + 1, 
			szId);

		_ASSERTE(SUCCEEDED(hr));
	}

	Device::~Device()
	{
		Clear();
	}

	DWORD Device::GetSlotNo()
	{
		return m_slot;
	}

	LPCTSTR Device::GetStringId()
	{
		return m_id;
	}

	NDAS_DEVICE_STATUS Device::GetStatus()
	{
		return m_status;
	}

	ACCESS_MASK Device::GetGrantedAccess()
	{
		return m_grantedAccess;
	}

	BOOL Device::SetAsReadOnly()
	{
		BOOL fSuccess = ::NdasSetDeviceAccessById(
			m_id, 
			FALSE,
			NULL);
		return fSuccess;
	}

	BOOL Device::SetAsReadWrite(LPCTSTR szKey)
	{
		BOOL fSuccess = ::NdasSetDeviceAccessById(
			m_id,
			TRUE,
			szKey);
		return fSuccess;

	}

	BOOL Device::Enable(bool bEnable /* = true */)
	{
		BOOL fSuccess = ::NdasEnableDevice(m_slot, bEnable);
		return fSuccess;
	}

	LPCTSTR Device::GetName()
	{
		return m_name;
	}

	BOOL Device::SetName(LPCTSTR szName)
	{
		BOOL fSuccess = ::NdasSetDeviceName(m_slot, szName);
		return fSuccess;
	}

	void Device::Clear()
	{
		UnitDeviceVector::iterator itr = m_unitDevices.begin();
		while (itr != m_unitDevices.end()) {
			UnitDevice* pUnitDevice = *itr;
			pUnitDevice->Release();
			itr = m_unitDevices.erase(itr);
		}
		_ASSERTE(m_unitDevices.size() == 0);
	}

	const NDAS_DEVICE_HW_INFORMATION& Device::GetHwInfo()
	{
		if (NULL == m_hwinfo) {
			UpdateInfo();
		}
		return *m_hwinfo;
	}


	BOOL Device::UpdateStatus()
	{
		BOOL fSuccess = ::NdasQueryDeviceStatus(
			m_slot, 
			&m_status, 
			&m_lastError);

		return fSuccess;
	}

	DWORD Device::GetUnitDeviceCount()
	{
		return (DWORD) m_unitDevices.size();
	}

	UnitDevice* Device::GetUnitDevice(DWORD dwIndex)
	{
		if (dwIndex >= m_unitDevices.size()) {
			return NULL;
		}
		m_unitDevices[dwIndex]->AddRef();
		return m_unitDevices[dwIndex];
	}

	UnitDevice* Device::FindUnitDevice(DWORD dwUnitNo)
	{
		UnitDeviceVector::const_iterator itr = m_unitDevices.begin();
		while (itr != m_unitDevices.end()) {
			UnitDevice* pUnitDevice = *itr;
			if (pUnitDevice->GetUnitNo() == dwUnitNo) {
				pUnitDevice->AddRef();
				return pUnitDevice;
			}
			++itr;
		}
		return NULL;
	}

	BOOL Device::UpdateInfo()
	{
		Clear();

		NDASUSER_DEVICE_INFORMATION devInfo = {0};

		BOOL fSuccess = ::NdasQueryDeviceInformation(m_slot, &devInfo);
		
		if (!fSuccess) {
			return FALSE;
		}

		m_grantedAccess = devInfo.GrantedAccess;
		m_hwinfo = new NDAS_DEVICE_HW_INFORMATION;
		::CopyMemory(
			m_hwinfo, 
			&devInfo.HardwareInfo, 
			sizeof(NDAS_DEVICE_HW_INFORMATION));

		_ASSERTE(m_slot == devInfo.SlotNo);

		::CopyMemory(
			m_id,
            devInfo.szDeviceId,
			NDAS_DEVICE_STRING_ID_LEN);

		::CopyMemory(
			m_name,
            devInfo.szDeviceName,
			MAX_NDAS_DEVICE_NAME_LEN);

		
		fSuccess = ::NdasEnumUnitDevices(
			m_slot, 
			UnitDeviceEnumProc, 
			reinterpret_cast<LPVOID>(this));
		
		if (!fSuccess) {
			return FALSE;
		}

		return TRUE;
	}

	BOOL CALLBACK Device::UnitDeviceEnumProc(
			PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry, 
			LPVOID lpContext)
	{
		Device* pDevice = reinterpret_cast<Device*>(lpContext);
		lpEntry->UnitDeviceType;
		lpEntry->UnitNo;

		UnitDevice* pUnitDevice = new UnitDevice(
			pDevice,
			lpEntry->UnitNo,
			lpEntry->UnitDeviceType);

		pUnitDevice->AddRef();
		pDevice->m_unitDevices.push_back(pUnitDevice);

		return TRUE;
	}

	///////////////////////////////////////////////////////////////////////
	// Unit Device
	///////////////////////////////////////////////////////////////////////

	UnitDevice::UnitDevice(
		Device* const pDevice,
		DWORD unitNo, 
		NDAS_UNITDEVICE_TYPE type) :
		m_pDevice(pDevice),
		m_unitNo(unitNo),
		m_type(type),
		m_hwinfo(NULL),
		m_status(NDAS_UNITDEVICE_STATUS_UNKNOWN),
		m_lastError(NDAS_UNITDEVICE_ERROR_NONE),
		m_bInfoUpdated(false),
		m_bStatusUpdated(false)
	{
	}

	UnitDevice::~UnitDevice()
	{
		if (NULL != m_hwinfo) {
			delete m_hwinfo;
		}
	}

	Device* UnitDevice::GetParentDevice()
	{
		m_pDevice->AddRef();
		return m_pDevice;
	}

	DWORD UnitDevice::GetSlotNo()
	{
		return m_pDevice->GetSlotNo();
	}

	DWORD UnitDevice::GetUnitNo()
	{
		return m_unitNo;
	}

	const NDAS_UNITDEVICE_HW_INFORMATION& UnitDevice::GetHWInfo()
	{
		if (!m_bInfoUpdated) {
			UpdateInfo();
		}
		return *m_hwinfo;
	}

	const NDAS_UNITDEVICE_USAGE_STATS& UnitDevice::GetUsage()
	{
		if (!m_bInfoUpdated) {
			UpdateInfo();
		}
		return m_stats;
	}

	NDAS_UNITDEVICE_TYPE UnitDevice::GetType()
	{
		return m_type;
	}

	NDAS_UNITDEVICE_SUBTYPE UnitDevice::GetSubType()
	{
		if (!m_bInfoUpdated) {
			UpdateInfo();
		}
		return m_subtype;
	}

	NDAS_UNITDEVICE_STATUS UnitDevice::GetStatus()
	{
		if (!m_bStatusUpdated) {
			UpdateStatus();
		}
		return m_status;
	}

	BOOL UnitDevice::UpdateInfo()
	{
		NDASUSER_UNITDEVICE_INFORMATION unitDeviceInfo;
		
		BOOL fSuccess = ::NdasQueryUnitDeviceInformation(
			m_pDevice->GetSlotNo(),
			m_unitNo,
			&unitDeviceInfo);

		if (!fSuccess) {
			return FALSE;
		}
		
		m_hwinfo = new NDAS_UNITDEVICE_HW_INFORMATION;
		::CopyMemory(
			m_hwinfo, 
			&unitDeviceInfo.HardwareInfo,
			sizeof(NDAS_UNITDEVICE_HW_INFORMATION));

		m_type = unitDeviceInfo.UnitDeviceType;
		m_subtype = unitDeviceInfo.UnitDeviceSubType;
		m_stats = unitDeviceInfo.UsageStats;

		m_bInfoUpdated = true;

		return TRUE;
	}

	BOOL UnitDevice::UpdateStatus()
	{
		BOOL fSuccess = ::NdasQueryUnitDeviceStatus(
			m_pDevice->GetSlotNo(),
			m_unitNo,
			&m_status,
			&m_lastError);

		if (!fSuccess) {
			return FALSE;
		}

		m_bStatusUpdated = true;

		return TRUE;
	}

	NDAS_LOGICALDEVICE_ID UnitDevice::GetLogicalDeviceId()
	{
		NDAS_LOGICALDEVICE_ID logicalDeviceId;
		BOOL fSuccess = ::NdasFindLogicalDeviceOfUnitDevice(
			GetSlotNo(), GetUnitNo(), &logicalDeviceId);
		if (!fSuccess) {
			return NullNdasLogicalDeviceId();
		}

		return logicalDeviceId;
	}

	///////////////////////////////////////////////////////////////////////
	// LogicalDeviceCollection class
	///////////////////////////////////////////////////////////////////////

	LogicalDeviceColl::LogicalDeviceColl()
	{
	}

	LogicalDeviceColl::~LogicalDeviceColl()
	{
		Clear();
	}

	LogicalDevice* LogicalDeviceColl::FindLogicalDevice(
			const NDAS_LOGICALDEVICE_ID& logicalDeviceId)
	{
		LogicalDeviceVector::const_iterator itr = m_logicalDevices.begin();
		for(; itr != m_logicalDevices.end(); ++itr) {
			LogicalDevice* pLogDev = *itr;
			if (pLogDev->GetSlotNo() == logicalDeviceId.SlotNo &&
				pLogDev->GetTargetId() == logicalDeviceId.TargetId &&
				pLogDev->GetLUN() == logicalDeviceId.LUN)
			{
				pLogDev->AddRef();
				return pLogDev;
			}
		}
		return NULL;
	}

	LogicalDevice* LogicalDeviceColl::GetLogicalDevice(DWORD dwIndex)
	{
		LogicalDevice* pLogDev = m_logicalDevices.at(dwIndex);
		pLogDev->AddRef();
		return pLogDev;
	}
    
	DWORD LogicalDeviceColl::GetLogicalDeviceCount()
	{
		return (DWORD) m_logicalDevices.size();
	}

	void LogicalDeviceColl::Clear()
	{
		LogicalDeviceVector::iterator itr = m_logicalDevices.begin();
		while (itr != m_logicalDevices.end()) {
			LogicalDevice* pLogDev = *itr;
			pLogDev->Release();
			itr = m_logicalDevices.erase(itr);
		}
		_ASSERTE(m_logicalDevices.size() == 0);
	}

	BOOL CALLBACK LogicalDeviceColl::EnumProc(
		PNDASUSER_LOGICALDEVICE_ENUM_ENTRY lpEntry, 
		LPVOID lpContext)
	{
		LogicalDeviceColl* pLogDevColl =
			reinterpret_cast<LogicalDeviceColl*>(lpContext);

		LogicalDevice* pLogDev = new LogicalDevice(
			lpEntry->LogicalDeviceId,
			lpEntry->Type);

		pLogDev->AddRef();
		pLogDevColl->m_logicalDevices.push_back(pLogDev);

		return TRUE;
	}

	BOOL LogicalDeviceColl::Update()
	{
		Clear();
		BOOL fSuccess = ::NdasEnumLogicalDevices(
			LogicalDeviceColl::EnumProc,
			reinterpret_cast<LPVOID>(this));
		return fSuccess;
	}

	///////////////////////////////////////////////////////////////////////
	// LogicalDevice class
	///////////////////////////////////////////////////////////////////////

	LogicalDevice::LogicalDevice(
		const NDAS_LOGICALDEVICE_ID& logicalDeviceId,
		NDAS_LOGICALDEVICE_TYPE type) :
		m_type(type),
		m_id(logicalDeviceId),
		m_status(NDAS_LOGICALDEVICE_STATUS_UNKNOWN)
	{
	}

	NDAS_LOGICALDEVICE_ID LogicalDevice::GetLogicalDeviceId()
	{
		return m_id;
	}

	DWORD LogicalDevice::GetSlotNo()
	{
		return m_id.SlotNo;
	}

	DWORD LogicalDevice::GetTargetId()
	{
		return m_id.TargetId;
	}

	DWORD LogicalDevice::GetLUN()
	{
		return m_id.LUN;
	}

	NDAS_LOGICALDEVICE_TYPE LogicalDevice::GetType()
	{
		return m_type;
	}

	BOOL LogicalDevice::PlugIn(bool bWritable /* = false */)
	{
		BOOL fSuccess = ::NdasPlugInLogicalDevice(
			bWritable,
			m_id);
		return fSuccess;
	}

	BOOL LogicalDevice::Eject()
	{
		BOOL fSuccess = ::NdasEjectLogicalDevice(m_id);
		return fSuccess;
	}

	BOOL LogicalDevice::Unplug()
	{
		BOOL fSuccess = ::NdasUnplugLogicalDevice(m_id);
		return fSuccess;
	}

	BOOL LogicalDevice::UpdateInfo()
	{
		NDASUSER_LOGICALDEVICE_INFORMATION info = {0};
		BOOL fSuccess = ::NdasQueryLogicalDeviceInformation(
			m_id,
			&info);
		if (!fSuccess) {
			return FALSE;
		}
		m_grantedAccess = info.GrantedAccess;
		m_mountedAccess = info.MountedAccess;
		m_type = info.LogicalDeviceType;

		if (info.nUnitDeviceEntries > 1) {

			m_unitDevices.clear();

			BOOL fSuccess = ::NdasEnumLogicalDeviceMembers(
				m_id,
				MemberEnumProc,
				reinterpret_cast<LPVOID>(this));
			if (!fSuccess) {
				return FALSE;
			}

		} else {

			m_unitDevices.clear();

			UNITDEVICE_INFO ui = {
				0,
				{0},
				info.FirstUnitDevice.UnitNo,
				info.FirstUnitDevice.Blocks };
		
			::CopyMemory(
				ui.DeviceId, 
				info.FirstUnitDevice.szDeviceStringId, 
				sizeof(ui.DeviceId));
			
			m_unitDevices.push_back(ui);
			
		}

		return TRUE;
	}

	NDAS_LOGICALDEVICE_STATUS LogicalDevice::GetStatus()
	{
		return m_status;
	}

	ACCESS_MASK LogicalDevice::GetGrantedAccess()
	{
		return m_grantedAccess;
	}

	ACCESS_MASK LogicalDevice::GetMountedAccess()
	{
		return m_mountedAccess;
	}

	const LogicalDevice::UNITDEVICE_INFO& 
	LogicalDevice::
	GetUnitDeviceInfo(DWORD dwIndex)
	{
		return m_unitDevices.at(dwIndex);
	}

	DWORD LogicalDevice::GetUnitDeviceInfoCount()
	{
		return static_cast<DWORD>(m_unitDevices.size());
	}

	BOOL LogicalDevice::UpdateStatus()
	{
		BOOL fSuccess = ::NdasQueryLogicalDeviceStatus(
			m_id, 
			&m_status,
			&m_lastError);
		return fSuccess;
	}

	BOOL CALLBACK LogicalDevice::MemberEnumProc(
		PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY lpEntry, 
		LPVOID lpContext)
	{
		LogicalDevice* pThis = reinterpret_cast<LogicalDevice*>(lpContext);

		UNITDEVICE_INFO info = {
			lpEntry->Index,
			{0},
			lpEntry->UnitNo,
			lpEntry->Blocks };

		::CopyMemory(
			info.DeviceId, 
			lpEntry->szDeviceStringId, 
			sizeof(info.DeviceId) / sizeof(info.DeviceId[0]));

		pThis->m_unitDevices.push_back(info);

		return TRUE;
	}

}
