#include "stdafx.h"
#include "ndascls.h"

namespace ndas {

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
		ATLASSERT(m_devices.size() == 0);
	}

	BOOL DeviceColl::Update()
	{
		if (!IsExpired()) { return TRUE; }

		OnUpdate();
		Clear();
		BOOL fSuccess = ::NdasEnumDevicesW(
			DeviceColl::EnumProc, 
			reinterpret_cast<LPVOID>(this));
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasEnumDevices failed: %08X\n"), ::GetLastError());
			::SetLastError(err);
		}

		return fSuccess;
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

	Device* DeviceColl::FindDeviceByName(LPCTSTR szName)
	{
		DeviceVector::const_iterator itr = m_devices.begin();
		while (itr != m_devices.end()) {
			Device* pDevice = *itr;
			if (0 == lstrcmpi(pDevice->GetName(), szName)) {
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
		// not implemented yet!
		ATLASSERT(FALSE);
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

		ATLASSERT(SUCCEEDED(hr));

		::ZeroMemory(
			m_id,
			NDAS_DEVICE_STRING_ID_LEN + 1);

		hr = ::StringCchCopy(
			m_id, 
			NDAS_DEVICE_STRING_ID_LEN + 1, 
			szId);

		/* clear all validity bits */
		m_validity.Value = 0;

		ATLASSERT(SUCCEEDED(hr));
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

	NDAS_DEVICE_ERROR Device::GetLastError()
	{
		return m_lastError;
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
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasSetDeviceAccessById To RO failed: %08X\n"), 
				::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		m_grantedAccess = GENERIC_READ;
		return TRUE;
	}

	BOOL Device::SetAsReadWrite(LPCTSTR szKey)
	{
		BOOL fSuccess = ::NdasSetDeviceAccessById(
			m_id,
			TRUE,
			szKey);

		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasSetDeviceAccessById To RW failed: %08X\n"), 
				::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		m_grantedAccess = GENERIC_READ | GENERIC_WRITE;
		return TRUE;
	}

	BOOL Device::Enable(BOOL bEnable /* = true */)
	{
		BOOL fSuccess = ::NdasEnableDevice(m_slot, bEnable);

		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasEnableDevice %d at slot %d failed: %08X\n"), 
				bEnable, m_slot, ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		(VOID) UpdateStatus();
		return TRUE;
	}

	LPCTSTR Device::GetName()
	{
		return m_name;
	}

	BOOL Device::SetName(LPCTSTR szName)
	{
		BOOL fSuccess = ::NdasSetDeviceName(m_slot, szName);
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasSetDeviceName to %s at slot %d failed: %08X\n"),
				szName, m_slot, ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}

		HRESULT hr = ::StringCchCopy(
			m_name, 
			RTL_NUMBER_OF(m_name), 
			szName);
		ATLASSERT(SUCCEEDED(hr));

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

		/* clear validity bits */
		m_validity.Value = 0;
	}

	const NDAS_DEVICE_HW_INFORMATION* Device::GetHwInfo()
	{
		return (m_validity.HardwareInfo) ? &m_hwinfo : NULL;
	}

	const NDAS_DEVICE_PARAMS* Device::GetDeviceParams()
	{
		return (m_validity.DeviceParams) ? &m_params : NULL;
	}


	BOOL Device::UpdateStatus()
	{
		BOOL fSuccess = ::NdasQueryDeviceStatus(
			m_slot, 
			&m_status, 
			&m_lastError);

		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasQueryDeviceStatus to slot %d failed: %08X\n"),
				m_slot, ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}

		return TRUE;
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

	BOOL Device::IsAnyUnitDeviceMounted()
	{
		UnitDeviceVector::const_iterator itr = m_unitDevices.begin();
		while (itr != m_unitDevices.end()) {
			UnitDevice* pUnitDevice = *itr;
			if (NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus()) {
				return TRUE;
			}
			++itr;
		}
		return FALSE;
	}

	BOOL Device::UpdateInfo()
	{
		Clear();

		NDASUSER_DEVICE_INFORMATION devInfo = {0};

		BOOL fSuccess = ::NdasQueryDeviceInformation(m_slot, &devInfo);
		
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasQueryDeviceInformation at slot %d failed: %08X.\n"),
				m_slot, ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}

		m_grantedAccess = devInfo.GrantedAccess;

		::CopyMemory(
			&m_hwinfo,
			&devInfo.HardwareInfo, 
			sizeof(NDAS_DEVICE_HW_INFORMATION));
		m_validity.HardwareInfo = TRUE;

		_ASSERTE(m_slot == devInfo.SlotNo);

		::CopyMemory(
			m_id,
            devInfo.szDeviceId,
			NDAS_DEVICE_STRING_ID_LEN);

		::CopyMemory(
			m_name,
            devInfo.szDeviceName,
			MAX_NDAS_DEVICE_NAME_LEN);

		::CopyMemory(
			&m_params, 
			&devInfo.DeviceParams,
			sizeof(NDAS_DEVICE_PARAMS));

		m_validity.DeviceParams = TRUE;

		fSuccess = ::NdasEnumUnitDevices(
			m_slot, 
			UnitDeviceEnumProc, 
			reinterpret_cast<LPVOID>(this));
		
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasEnumUnitDevices at slot %d failed: %08X.\n"),
				m_slot, ::GetLastError());
			::SetLastError(err);
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
		m_pHWInfo(NULL),
		m_nROHosts(0),
		m_nRWHosts(0),
		m_status(NDAS_UNITDEVICE_STATUS_UNKNOWN),
		m_lastError(NDAS_UNITDEVICE_ERROR_NONE)
	{
	}

	UnitDevice::~UnitDevice()
	{
		if (NULL != m_pHWInfo) {
			delete m_pHWInfo;
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

	CONST NDAS_UNITDEVICE_HW_INFORMATION* UnitDevice::GetHWInfo()
	{
		return m_pHWInfo;
	}

	NDAS_UNITDEVICE_TYPE UnitDevice::GetType()
	{
		return m_type;
	}

	NDAS_UNITDEVICE_SUBTYPE UnitDevice::GetSubType()
	{
		return m_subtype;
	}

	NDAS_UNITDEVICE_STATUS UnitDevice::GetStatus()
	{
		return m_status;
	}

	BOOL UnitDevice::UpdateInfo()
	{
		NDASUSER_UNITDEVICE_INFORMATION unitDeviceInfo;
		
		DWORD dwSlotNo = m_pDevice->GetSlotNo();
		BOOL fSuccess = ::NdasQueryUnitDeviceInformation(
			dwSlotNo,
			m_unitNo,
			&unitDeviceInfo);

		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasQueryUnitDeviceInformation at %d.%d failed: %08X.\n"),
				dwSlotNo, m_unitNo, ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		
		m_pHWInfo = new NDAS_UNITDEVICE_HW_INFORMATION;

		::CopyMemory(
			m_pHWInfo, 
			&unitDeviceInfo.HardwareInfo,
			sizeof(NDAS_UNITDEVICE_HW_INFORMATION));

		m_type = unitDeviceInfo.UnitDeviceType;
		m_subtype = unitDeviceInfo.UnitDeviceSubType;

		m_bInfoUpdated = true;

		return TRUE;
	}

	BOOL UnitDevice::UpdateStatus()
	{
		DWORD dwSlotNo = m_pDevice->GetSlotNo();
		BOOL fSuccess = ::NdasQueryUnitDeviceStatus(
			dwSlotNo,
			m_unitNo,
			&m_status,
			&m_lastError);

		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasQueryUnitDeviceStatus at %d.%d failed: %08X.\n"),
				dwSlotNo, m_unitNo, ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}

		return TRUE;
	}

	BOOL UnitDevice::UpdateHostStats()
	{
		DWORD dwSlotNo = m_pDevice->GetSlotNo();
		BOOL fSuccess = ::NdasQueryUnitDeviceHostStats(
			dwSlotNo, 
			m_unitNo, 
			&m_nROHosts, 
			&m_nRWHosts);

		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasQueryUnitDeviceHostStats at %d.%d failed: %08X.\n"),
				dwSlotNo, m_unitNo, ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}

		return TRUE;
	}

	NDAS_LOGICALDEVICE_ID UnitDevice::GetLogicalDeviceId()
	{
		NDAS_LOGICALDEVICE_ID logicalDeviceId;
		BOOL fSuccess = ::NdasFindLogicalDeviceOfUnitDevice(
			GetSlotNo(), GetUnitNo(), &logicalDeviceId);
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasFindLogicalDeviceOfUnitDevice %d.%d failed: %08X.\n"),
				GetSlotNo(), GetUnitNo(), ::GetLastError());
			::SetLastError(err);
			return INVALID_NDAS_LOGICALDEVICE_ID;
		}

		return logicalDeviceId;
	}

	BOOL UnitDevice::QueryHostEnumProc(
		LPCGUID lpHostGuid,
		ACCESS_MASK Access)
	{
		UNITDEVICE_HOST_ENTRY hostEntry;
		hostEntry.HostGuid = *lpHostGuid;
		hostEntry.Access = Access;
		BOOL fSuccess = ::NdasQueryHostInfo(lpHostGuid,&hostEntry.HostInfo);
		if (!fSuccess) {
			// continue enumeration
			return TRUE;
		}

		m_hostInfoEntries.push_back(hostEntry);
		return TRUE;
	}

	BOOL UnitDevice::UpdateHostInfo()
	{

		UpdateInfo();
		m_hostInfoEntries.clear();
		BOOL fSuccess = ::NdasQueryHostsForUnitDevice(
			m_pDevice->GetSlotNo(),
			m_unitNo,
			spQueryHostEnumProc,
			this);

		return fSuccess;
	}

	DWORD UnitDevice::GetHostInfoCount()
	{
		return m_hostInfoEntries.size();
	}

	CONST NDAS_HOST_INFO*
	UnitDevice::GetHostInfo(
		DWORD dwIndex, 
		ACCESS_MASK* lpAccess,
		LPGUID lpHostGuid)
	{
		if (dwIndex >= m_hostInfoEntries.size()) {
			return NULL;
		}
		CONST UNITDEVICE_HOST_ENTRY& hostEntry = m_hostInfoEntries.at(dwIndex);
		if (lpAccess) *lpAccess = hostEntry.Access;
		if (lpHostGuid) *lpHostGuid = hostEntry.HostGuid;
		return &hostEntry.HostInfo;
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
			NDAS_LOGICALDEVICE_ID logicalDeviceId)
	{
		LogicalDeviceVector::const_iterator itr = m_logicalDevices.begin();
		for(; itr != m_logicalDevices.end(); ++itr) {
			LogicalDevice* pLogDev = *itr;
			if (pLogDev->GetLogicalDeviceId() == logicalDeviceId) {
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
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasEnumLogicalDevices failed: %08X.\n"),
				::GetLastError());
			::SetLastError(err);
		}
		return fSuccess;
	}

	///////////////////////////////////////////////////////////////////////
	// LogicalDevice class
	///////////////////////////////////////////////////////////////////////

	LogicalDevice::LogicalDevice(
		NDAS_LOGICALDEVICE_ID logicalDeviceId,
		NDAS_LOGICALDEVICE_TYPE type) :
		m_type(type),
		m_id(logicalDeviceId),
		m_status(NDAS_LOGICALDEVICE_STATUS_UNKNOWN)
	{
		::ZeroMemory(
			&m_logDeviceInfo, 
			sizeof(NDASUSER_LOGICALDEVICE_INFORMATION));
	}

	NDAS_LOGICALDEVICE_ID LogicalDevice::GetLogicalDeviceId()
	{
		return m_id;
	}

	NDAS_LOGICALDEVICE_TYPE LogicalDevice::GetType()
	{
		return m_type;
	}

	BOOL LogicalDevice::PlugIn(BOOL bWritable /* = false */)
	{
		BOOL fSuccess = ::NdasPlugInLogicalDevice(
			bWritable,
			m_id);

		if (!fSuccess) {
			//
			// Side-effect: ATLTRACE will invoke OutputDebugString
			// If debugger is not attached, the last error will be 2
			// always (ERROR_FILE_NOT_FOUND)
			//
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasPlugInLogicalDevice as %s failed: %08X.\n"),
				bWritable ? _T("RW") : _T("RO"), ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		return TRUE;
	}

	BOOL LogicalDevice::Eject()
	{
		BOOL fSuccess = ::NdasEjectLogicalDevice(m_id);
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasEjectLogicalDevice failed: %08X.\n"),
				::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		return TRUE;
	}

	BOOL LogicalDevice::Unplug()
	{
		BOOL fSuccess = ::NdasUnplugLogicalDevice(m_id);
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasUnplugLogicalDevice failed: %08X.\n"),
				::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		return TRUE;
	}

	BOOL LogicalDevice::UpdateInfo()
	{
		BOOL fSuccess = ::NdasQueryLogicalDeviceInformation(
			m_id,
			&m_logDeviceInfo);
		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasQueryLogicalDeviceInformation failed: %08X.\n"),
				::GetLastError());
			::SetLastError(err);
			return FALSE;
		}

		if (m_logDeviceInfo.nUnitDeviceEntries > 1) {

			m_unitDevices.clear();

			BOOL fSuccess = ::NdasEnumLogicalDeviceMembers(
				m_id,
				MemberEnumProc,
				reinterpret_cast<LPVOID>(this));
			if (!fSuccess) {
				DWORD err = ::GetLastError();
				ATLTRACE(_T("NdasEnumLogicalDeviceMembers failed: %08X.\n"),
					::GetLastError());
				::SetLastError(err);
				return FALSE;
			}

		} else {

			m_unitDevices.clear();

			UNITDEVICE_INFO ui = {
				0,
				{0},
				m_logDeviceInfo.FirstUnitDevice.UnitNo,
				m_logDeviceInfo.FirstUnitDevice.Blocks };
		
			::CopyMemory(
				ui.DeviceId, 
				m_logDeviceInfo.FirstUnitDevice.szDeviceStringId, 
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
		return m_logDeviceInfo.GrantedAccess;
	}

	ACCESS_MASK LogicalDevice::GetMountedAccess()
	{
		return m_logDeviceInfo.MountedAccess;
	}

	CONST LogicalDevice::UNITDEVICE_INFO& 
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

		if (!fSuccess) {
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasQueryLogicalDeviceStatus failed: %d(0x%08X)\n"),
				::GetLastError(), ::GetLastError());
			::SetLastError(err);
			return FALSE;
		}
		return TRUE;
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
			sizeof(info.DeviceId));

		pThis->m_unitDevices.push_back(info);

		return TRUE;
	}

	const NDASUSER_LOGICALDEVICE_INFORMATION* 
	LogicalDevice::
	GetLogicalDeviceInfo()
	{
		return &m_logDeviceInfo;
	}

	BOOL
	LogicalDevice::
	IsContentEncrypted()
	{
		if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_type))
		{
			return FALSE;
		}
		if (NDAS_CONTENT_ENCRYPT_TYPE_NONE != 
			m_logDeviceInfo.LogicalDiskInformation.ContentEncrypt.Type)
		{
			return TRUE;
		}
		return FALSE;
	}


}
