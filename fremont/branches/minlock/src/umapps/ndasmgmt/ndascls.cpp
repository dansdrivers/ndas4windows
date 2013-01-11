#include "stdafx.h"
#include "ndascls.h"
#include <algorithm>
#include <setupapi.h>

namespace ndas {

DeviceVector& GetDevicesInternal()
{
	static DeviceVector devices;
	return devices;
}

LogicalDeviceVector& GetLogicalDevicesInternal()
{
	static LogicalDeviceVector logDevices;
	return logDevices;
}

const DeviceVector& GetDevices()
{
	return GetDevicesInternal();
}

const LogicalDeviceVector& GetLogicalDevices()
{
	return GetLogicalDevicesInternal();
}

///////////////////////////////////////////////////////////////////////
// Device Collection class
///////////////////////////////////////////////////////////////////////

namespace
{

BOOL CALLBACK 
EnumDeviceProc(
	PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, 
	LPVOID lpContext)
{
	DeviceVector* pDeviceList = reinterpret_cast<DeviceVector*>(lpContext);
	DevicePtr pDevice(
		new Device(
			lpEnumEntry->SlotNo,
			lpEnumEntry->szDeviceStringId,
			lpEnumEntry->szDeviceName,
			lpEnumEntry->GrantedAccess));

	pDeviceList->push_back(pDevice);
	pDevice->UpdateStatus();
	pDevice->UpdateInfo();
	return TRUE;
}

BOOL CALLBACK
EnumLogicalDeviceProc(
	PNDASUSER_LOGICALDEVICE_ENUM_ENTRY lpEntry, 
	LPVOID lpContext)
{
	LogicalDeviceVector* pLogDeviceList = 
		reinterpret_cast<LogicalDeviceVector*>(lpContext);
	LogicalDevicePtr pLogDevice(
		new LogicalDevice(
			lpEntry->LogicalDeviceId,
			lpEntry->Type));
	pLogDeviceList->push_back(pLogDevice);
	pLogDevice->UpdateStatus();
	pLogDevice->UpdateInfo();
	return TRUE;
}

}

bool
UpdateDeviceList()
{
	DeviceVector& devices = GetDevicesInternal();
	devices.clear();
	if (!NdasEnumDevices(EnumDeviceProc, &devices))
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasEnumDevices failed: %08X\n", ::GetLastError());
		::SetLastError(err);
		return false;
	}
	std::sort(
		devices.begin(), 
		devices.end(), 
		DeviceNameSortAscending());
	return true;
}

bool
UpdateLogicalDeviceList()
{
	LogicalDeviceVector& logDevices = GetLogicalDevicesInternal();
	logDevices.clear();
	if (!NdasEnumLogicalDevices(EnumLogicalDeviceProc, &logDevices))
	{
		return false;
	}
	return true;
}

template <typename Pred>
bool FindDevice(DevicePtr& p, Pred pred)
{
	const DeviceVector& v = GetDevices();
	DeviceConstIterator itr = 
		std::find_if(v.begin(), v.end(), pred);
	if (v.end() == itr)
	{
		return false;
	}
	p = *itr;
	return true;
}

bool FindDeviceBySlotNumber(DevicePtr& p, DWORD SlotNumber)
{
	return FindDevice(p, DeviceSlotNumberEquals(SlotNumber));
}

bool FindDeviceByNdasId(DevicePtr& p, LPCTSTR szNdasId)
{
	return FindDevice(p, DeviceIdEquals(szNdasId));
}

bool FindDeviceByName(DevicePtr& p, LPCTSTR Name)
{
	return FindDevice(p, DeviceNameEquals(Name));
}

bool FindLogicalDevice(LogicalDevicePtr& p, NDAS_LOGICALDEVICE_ID Id)
{
	const LogicalDeviceVector& v = GetLogicalDevices();
	LogicalDeviceIdEquals pred(Id);
	LogicalDeviceConstIterator itr = 
		std::find_if<LogicalDeviceConstIterator, LogicalDeviceIdEquals&>(
			v.begin(), v.end(), pred);
	if (v.end() == itr)
	{
		return false;
	}
	p = *itr;
	return true;
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

	ATLASSERT(SUCCEEDED(hr));
}

DWORD 
Device::GetSlotNo()
{
	return m_slot;
}

LPCTSTR 
Device::GetStringId()
{
	return m_id;
}

NDAS_DEVICE_STATUS 
Device::GetStatus()
{
	return m_status;
}

NDAS_DEVICE_ERROR 
Device::GetLastError()
{
	return m_lastError;
}

ACCESS_MASK 
Device::GetGrantedAccess()
{
	return m_grantedAccess;
}

BOOL 
Device::SetAsReadOnly()
{
	BOOL fSuccess = ::NdasSetDeviceAccessById(
		m_id, 
		FALSE,
		NULL);
	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasSetDeviceAccessById To RO failed: %08X\n", 
			::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	m_grantedAccess = GENERIC_READ;
	return TRUE;
}

BOOL 
Device::SetAsReadWrite(LPCTSTR szKey)
{
	BOOL fSuccess = ::NdasSetDeviceAccessById(
		m_id,
		TRUE,
		szKey);

	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasSetDeviceAccessById To RW failed: %08X\n", 
			::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	m_grantedAccess = GENERIC_READ | GENERIC_WRITE;
	return TRUE;
}

BOOL 
Device::Enable(BOOL bEnable /* = true */)
{
	BOOL fSuccess = ::NdasEnableDevice(m_slot, bEnable);

	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasEnableDevice %d at slot %d failed: %08X\n", 
			bEnable, m_slot, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	(void) UpdateStatus();
	return TRUE;
}

LPCTSTR 
Device::GetName()
{
	return m_name;
}

BOOL 
Device::SetName(LPCTSTR szName)
{
	BOOL fSuccess = ::NdasSetDeviceName(m_slot, szName);
	if (!fSuccess) {
		DWORD err = ::GetLastError();
		ATLTRACE("NdasSetDeviceName to %s at slot %d failed: %08X\n",
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

NdasDeviceHardwareInfoConstPtr
Device::GetHardwareInfo()
{
	return m_pHardwareInfo;
}


BOOL 
Device::UpdateStatus()
{
	BOOL fSuccess = ::NdasQueryDeviceStatus(
		m_slot, 
		&m_status, 
		&m_lastError);

	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryDeviceStatus to slot %d failed: %08X\n",
			m_slot, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}

	return TRUE;
}

BOOL 
Device::UpdateStats()
{
	m_dstats.Size = sizeof(NDAS_DEVICE_STAT);
	BOOL fSuccess = ::NdasQueryDeviceStats(m_slot, &m_dstats);
	if (!fSuccess)
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryDeviceStats(%d) failed: %08X\n", 
			m_slot, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	return TRUE;
}

bool 
Device::FindUnitDevice(UnitDevicePtr& p, DWORD UnitNo)
{
	UnitDeviceVector::const_iterator itr = 
		std::find_if(
		m_unitDevices.begin(), 
		m_unitDevices.end(), 
		UnitDeviceNumberEqual(UnitNo) );
	if (m_unitDevices.end() == itr)
	{
		return false;
	}
	p = *itr;
	return true;
}

bool
Device::IsAnyUnitDeviceMounted()
{
	UnitDeviceVector::const_iterator itr = 
		std::find_if(
			m_unitDevices.begin(), 
			m_unitDevices.end(), 
			UnitDeviceMounted());
	return (itr != m_unitDevices.end()) ? true : false;
}

BOOL 
Device::UpdateInfo()
{
	m_unitDevices.clear();

	NDASUSER_DEVICE_INFORMATION devInfo = {0};

	BOOL fSuccess = ::NdasQueryDeviceInformation(m_slot, &devInfo);
	
	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryDeviceInformation at slot %d failed: %08X.\n",
			m_slot, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}

	m_grantedAccess = devInfo.GrantedAccess;
	m_pHardwareInfo = NdasDeviceHardwareInfoPtr(new NDAS_DEVICE_HARDWARE_INFO);

	::CopyMemory(
		m_pHardwareInfo.get(), 
		&devInfo.HardwareInfo, 
		sizeof(NDAS_DEVICE_HARDWARE_INFO));

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
		EnumUnitDeviceProc, 
		reinterpret_cast<LPVOID>(this));
	
	if (!fSuccess) {
		DWORD err = ::GetLastError();
		ATLTRACE("NdasEnumUnitDevices at slot %d failed: %08X.\n",
			m_slot, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}

	return TRUE;
}

BOOL 
Device::OnEnumUnitDevice(PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry)
{
	DevicePtr pThis;
	if (!FindDeviceBySlotNumber(pThis, m_slot))
	{
		ATLASSERT(FALSE && "Device entry is missing.");
		return TRUE;
	}

	UnitDevicePtr pUnitDevice(
		new UnitDevice(pThis, lpEntry->UnitNo, lpEntry->UnitDeviceType));
	m_unitDevices.push_back(pUnitDevice);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Unit Device
///////////////////////////////////////////////////////////////////////

UnitDevice::UnitDevice(
	DevicePtr pDevice,
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
	if (NULL != m_pHWInfo) 
{
		delete m_pHWInfo;
	}
}

DeviceWeakPtr 
UnitDevice::GetParentDevice()
{
	return m_pDevice;
}

DWORD 
UnitDevice::GetSlotNo()
{
	return DevicePtr(m_pDevice)->GetSlotNo();
}

DWORD 
UnitDevice::GetUnitNo()
{
	return m_unitNo;
}

const NDAS_UNITDEVICE_HARDWARE_INFO* 
UnitDevice::GetHWInfo()
{
	return m_pHWInfo;
}

NDAS_UNITDEVICE_TYPE 
UnitDevice::GetType()
{
	return m_type;
}

NDAS_UNITDEVICE_SUBTYPE 
UnitDevice::GetSubType()
{
	return m_subtype;
}

NDAS_UNITDEVICE_STATUS 
UnitDevice::GetStatus()
{
	return m_status;
}

NDAS_UNITDEVICE_ERROR
UnitDevice::GetLastError()
{
	return m_lastError;
}

BOOL 
UnitDevice::UpdateInfo()
{
	NDASUSER_UNITDEVICE_INFORMATION unitDeviceInfo;
	
	DWORD dwSlotNo = DevicePtr(m_pDevice)->GetSlotNo();
	BOOL fSuccess = ::NdasQueryUnitDeviceInformation(
		dwSlotNo,
		m_unitNo,
		&unitDeviceInfo);

	if (!fSuccess) {
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryUnitDeviceInformation at %d.%d failed: %08X.\n",
			dwSlotNo, m_unitNo, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	
	m_pHWInfo = new NDAS_UNITDEVICE_HARDWARE_INFO;

	::CopyMemory(
		m_pHWInfo, 
		&unitDeviceInfo.HardwareInfo,
		sizeof(NDAS_UNITDEVICE_HARDWARE_INFO));

	m_type = unitDeviceInfo.UnitDeviceType;
	m_subtype = unitDeviceInfo.UnitDeviceSubType;

	m_bInfoUpdated = true;

	return TRUE;
}

BOOL 
UnitDevice::UpdateStatus()
{
	DWORD dwSlotNo = DevicePtr(m_pDevice)->GetSlotNo();
	BOOL fSuccess = ::NdasQueryUnitDeviceStatus(
		dwSlotNo,
		m_unitNo,
		&m_status,
		&m_lastError);

	if (!fSuccess) {
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryUnitDeviceStatus at %d.%d failed: %08X.\n",
			dwSlotNo, m_unitNo, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}

	return TRUE;
}

BOOL 
UnitDevice::UpdateHostStats()
{
	DWORD dwSlotNo = DevicePtr(m_pDevice)->GetSlotNo();
	BOOL fSuccess = ::NdasQueryUnitDeviceHostStats(
		dwSlotNo, 
		m_unitNo, 
		&m_nROHosts, 
		&m_nRWHosts);

	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryUnitDeviceHostStats at %d.%d failed: %08X.\n",
			dwSlotNo, m_unitNo, ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}

	return TRUE;
}

NDAS_LOGICALDEVICE_ID 
UnitDevice::GetLogicalDeviceId()
{
	NDAS_LOGICALDEVICE_ID logicalDeviceId;
	BOOL fSuccess = ::NdasFindLogicalDeviceOfUnitDevice(
		GetSlotNo(), GetUnitNo(), &logicalDeviceId);
	if (!fSuccess) {
		DWORD err = ::GetLastError();
		ATLTRACE("NdasFindLogicalDeviceOfUnitDevice %d.%d failed: %08X.\n",
			GetSlotNo(), GetUnitNo(), ::GetLastError());
		::SetLastError(err);
		return INVALID_NDAS_LOGICALDEVICE_ID;
	}

	return logicalDeviceId;
}

BOOL 
UnitDevice::OnEnumHost(
	LPCGUID lpHostGuid,
	ACCESS_MASK Access)
{
	UNITDEVICE_HOST_ENTRY hostEntry;
	hostEntry.HostGuid = *lpHostGuid;
	hostEntry.Access = Access;
	BOOL fSuccess = ::NdasQueryHostInfo(lpHostGuid,&hostEntry.HostInfo);

	if (!fSuccess) 
	{
		// continue enumeration
		return TRUE;
	}

	m_hostInfoEntries.push_back(hostEntry);
	return TRUE;
}

BOOL 
UnitDevice::UpdateHostInfo()
{

	UpdateInfo();
	m_hostInfoEntries.clear();
	DevicePtr pDevice(GetParentDevice());
	BOOL fSuccess = ::NdasQueryHostsForUnitDevice(
		pDevice->GetSlotNo(),
		m_unitNo,
		EnumHostProc,
		this);

	return fSuccess;
}

DWORD 
UnitDevice::GetHostInfoCount()
{
	return m_hostInfoEntries.size();
}

const NDAS_HOST_INFO*
UnitDevice::GetHostInfo(
	DWORD dwIndex, 
	ACCESS_MASK* lpAccess,
	LPGUID lpHostGuid)
{
	if (dwIndex >= m_hostInfoEntries.size()) 
	{
		return NULL;
	}
	CONST UNITDEVICE_HOST_ENTRY& hostEntry = m_hostInfoEntries.at(dwIndex);
	if (lpAccess) *lpAccess = hostEntry.Access;
	if (lpHostGuid) *lpHostGuid = hostEntry.HostGuid;
	return &hostEntry.HostInfo;
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

NDAS_LOGICALDEVICE_ID 
LogicalDevice::GetLogicalDeviceId()
{
	return m_id;
}

NDAS_LOGICALDEVICE_TYPE 
LogicalDevice::GetType()
{
	return m_type;
}

BOOL 
LogicalDevice::PlugIn(BOOL bWritable /* = false */)
{
	BOOL fSuccess = ::NdasPlugInLogicalDevice(
		bWritable,
		m_id);

	if (!fSuccess) 
	{
		//
		// Side-effect: ATLTRACE will invoke OutputDebugString
		// If debugger is not attached, the last error will be 2
		// always (ERROR_FILE_NOT_FOUND)
		//
		DWORD err = ::GetLastError();
		ATLTRACE("NdasPlugInLogicalDevice as %s failed: %08X.\n",
			bWritable ? _T("RW") : _T("RO"), ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	return TRUE;
}

#define NDAS_FACILITY_CONFIGRET 0x1F0
#define NDAS_CUSTOMER_BIT (0x00A0 << 24)
__forceinline DWORD NDAS_CR_MAP(CONFIGRET cret)
{
	static const DWORD FacilityMask = NDAS_FACILITY_CONFIGRET << 16;
	static const DWORD CustomerMask = APPLICATION_ERROR_MASK;
	return (ERROR_SEVERITY_ERROR | CustomerMask | FacilityMask | cret);
}

DWORD
ConfigRetToWin32Error(CONFIGRET cret)
{
	static const struct { 
		CONFIGRET ConfigRet;
		DWORD Win32Error;
	} Mappings[] = {
		CR_SUCCESS, NO_ERROR,
		CR_OUT_OF_MEMORY, ERROR_NOT_ENOUGH_MEMORY,
		CR_INVALID_POINTER, ERROR_INVALID_USER_BUFFER,
		CR_INVALID_DEVNODE, ERROR_NO_SUCH_DEVINST,
		/* #define CR_INVALID_DEVINST CR_INVALID_DEVNODE */
		CR_NO_SUCH_DEVNODE, ERROR_DEVINST_ALREADY_EXISTS,
		/* #define CR_NO_SUCH_DEVINST CR_NO_SUCH_DEVNODE */
		CR_INVALID_DEVICE_ID, ERROR_INVALID_DEVINST_NAME,
		CR_ALREADY_SUCH_DEVNODE, ERROR_DEVINST_ALREADY_EXISTS,
		CR_INVALID_REFERENCE_STRING, ERROR_INVALID_REFERENCE_STRING,
		CR_INVALID_MACHINENAME, ERROR_INVALID_MACHINENAME,
		CR_REMOTE_COMM_FAILURE, ERROR_REMOTE_COMM_FAILURE,
		CR_NO_CM_SERVICES, ERROR_NO_CONFIGMGR_SERVICES,
		CR_ACCESS_DENIED, ERROR_ACCESS_DENIED,
		CR_CALL_NOT_IMPLEMENTED, ERROR_CALL_NOT_IMPLEMENTED,
		CR_NOT_DISABLEABLE, ERROR_NOT_DISABLEABLE
	};
	for (int i = 0; i < RTL_NUMBER_OF(Mappings); ++i)
	{
		if (Mappings[i].ConfigRet == cret)
		{
			return Mappings[i].Win32Error;
		}
	}
	return NDAS_CR_MAP(cret);
}

BOOL 
LogicalDevice::Eject()
{
	BOOL fSuccess = ::NdasEjectLogicalDevice(m_id);
	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasEjectLogicalDevice failed: %08X.\n",
			::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	return TRUE;
}

BOOL 
LogicalDevice::Eject(NDAS_LOGICALDEVICE_EJECT_PARAM* EjectParam)
{
	ZeroMemory(EjectParam, sizeof(NDAS_LOGICALDEVICE_EJECT_PARAM));
	EjectParam->Size = sizeof(NDAS_LOGICALDEVICE_EJECT_PARAM);
	EjectParam->LogicalDeviceId = m_id;
	BOOL fSuccess = ::NdasEjectLogicalDeviceEx(EjectParam);
	if (!fSuccess)
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasEjectLogicalDevice failed: %08X.\n",
			::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	// Success does not necessary mean that ejection is done.
	// ConfigRet should be CR_SUCCESS for success.
	// may return CR_REMOVE_VETOED if removal is vetoed.

	if (EjectParam->ConfigRet != CR_SUCCESS)
	{
		SetLastError(ConfigRetToWin32Error(EjectParam->ConfigRet));
		ATLTRACE("CR_%d-VetoType_%d-%ws\n", 
			EjectParam->ConfigRet,
			EjectParam->VetoType,
			EjectParam->VetoName);
		return FALSE;
	}
	return TRUE;
}

BOOL 
LogicalDevice::Unplug()
{
	BOOL fSuccess = ::NdasUnplugLogicalDevice(m_id);
	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasUnplugLogicalDevice failed: %08X.\n",
			::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	return TRUE;
}

BOOL 
LogicalDevice::UpdateInfo()
{
	BOOL fSuccess = ::NdasQueryLogicalDeviceInformation(
		m_id,
		&m_logDeviceInfo);
	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryLogicalDeviceInformation failed: %08X.\n",
			::GetLastError());
		::SetLastError(err);
		return FALSE;
	}

	if (m_logDeviceInfo.nUnitDeviceEntries > 1) 
	{

		m_unitDevices.clear();

		BOOL fSuccess = ::NdasEnumLogicalDeviceMembers(
			m_id,
			EnumMemberProc,
			reinterpret_cast<LPVOID>(this));
		
		if (!fSuccess) 
		{
			DWORD err = ::GetLastError();
			ATLTRACE("NdasEnumLogicalDeviceMembers failed: %08X.\n",
				::GetLastError());
			::SetLastError(err);
			return FALSE;
		}

	} 
	else 
	{

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

NDAS_LOGICALDEVICE_STATUS 
LogicalDevice::GetStatus()
{
	return m_status;
}

NDAS_LOGICALDEVICE_ERROR 
LogicalDevice::GetLastError(){
	return m_lastError;
}

ACCESS_MASK 
LogicalDevice::GetGrantedAccess()
{
	return m_logDeviceInfo.GrantedAccess;
}

ACCESS_MASK 
LogicalDevice::GetMountedAccess()
{
	return m_logDeviceInfo.MountedAccess;
}

const LogicalDevice::UNITDEVICE_INFO& 
LogicalDevice::GetUnitDeviceInfo(DWORD dwIndex)
{
	return m_unitDevices.at(dwIndex);
}

DWORD 
LogicalDevice::GetUnitDeviceInfoCount()
{
	return static_cast<DWORD>(m_unitDevices.size());
}

BOOL 
LogicalDevice::UpdateStatus()
{
	BOOL fSuccess = ::NdasQueryLogicalDeviceStatus(
		m_id, 
		&m_status,
		&m_lastError);

	if (!fSuccess) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("NdasQueryLogicalDeviceStatus failed: %d(0x%08X)\n",
			::GetLastError(), ::GetLastError());
		::SetLastError(err);
		return FALSE;
	}
	return TRUE;
}

BOOL 
LogicalDevice::OnEnumMember(PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY lpEntry)
{
	UNITDEVICE_INFO info = {
		lpEntry->Index,
		{0},
		lpEntry->UnitNo,
		lpEntry->Blocks };

	::CopyMemory(
			info.DeviceId, 
			lpEntry->szDeviceStringId, 
			sizeof(info.DeviceId));

	m_unitDevices.push_back(info);

	return TRUE;
}

const NDASUSER_LOGICALDEVICE_INFORMATION* 
LogicalDevice::GetLogicalDeviceInfo()
{
	return &m_logDeviceInfo;
}

DWORD 
LogicalDevice::GetLogicalDrives()
{
	return m_logDeviceInfo.LogicalDeviceParams.MountedLogicalDrives;
}

BOOL
LogicalDevice::IsContentEncrypted()
{
	if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK(m_type))
	{
		return FALSE;
	}
	if (NDAS_CONTENT_ENCRYPT_TYPE_NONE != 
		m_logDeviceInfo.SubType.LogicalDiskInformation.ContentEncrypt.Type)
	{
		return TRUE;
	}
	return FALSE;
}

CString 
LogicalDevice::GetName()
{
	DWORD count = GetUnitDeviceInfoCount();
	for (DWORD i = 0; i < count; ++i)
	{
		DevicePtr devicePtr;
		const UNITDEVICE_INFO& info = GetUnitDeviceInfo(i);
		if (FindDeviceByNdasId(devicePtr, info.DeviceId))
		{
			CString name = devicePtr->GetName();
			return name;
		}
	}
	return CString();	
}

void
GetMountedLogicalDevices(LogicalDeviceVector& dest)
{
	const LogicalDeviceVector& src = GetLogicalDevices();
	dest.reserve(src.size());
	std::remove_copy_if(
		src.begin(), 
		src.end(),
		dest.begin(),
		std::not1(LogicalDeviceMounted()));
}


} // namespace ndas

