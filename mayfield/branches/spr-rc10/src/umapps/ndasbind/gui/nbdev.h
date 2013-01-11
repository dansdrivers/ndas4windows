#pragma once

#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasdib.h>
#include <list>

class CNBNdasDevice;
class CNBDevice;
class CNBUnitDevice;
class CNBLogicalDevice;

typedef std::list<CNBNdasDevice *> NBNdasDevicePtrList;
typedef std::list<CNBDevice *> NBDevicePtrList;
typedef std::list<CNBUnitDevice *> NBUnitDevicePtrList;
typedef std::list<CNBLogicalDevice *> NBLogicalDevicePtrList;

typedef std::map<DWORD, CNBNdasDevice *> NBNdasDevicePtrMap;
typedef std::map<DWORD, CNBDevice *> NBDevicePtrMap;
typedef std::map<DWORD, CNBUnitDevice *> NBUnitDevicePtrMap;
typedef std::map<DWORD, CNBLogicalDevice *> NBLogicalDevicePtrMap;

// 1 per 1 NDAS Device
class CNBNdasDevice
{
	friend class CNBUnitDevice;
	friend class CNBLogicalDevice;
private:
	NBUnitDevicePtrMap m_mapUnitDevices;

	// device info
	NDASUSER_DEVICE_ENUM_ENTRY m_BaseInfo;
	NDAS_DEVICE_ID m_DeviceId;
	NDAS_DEVICE_STATUS m_status;

	BOOL			m_bServiceInfo; // TRUE if the information is retrieved from service
	NDAS_DEVICE_STATUS m_DeviceStatus;
public:
	CNBNdasDevice(PNDASUSER_DEVICE_ENUM_ENTRY pBaseInfo, NDAS_DEVICE_STATUS status);
	~CNBNdasDevice();
	
	BOOL UnitDeviceAdd(PNDASUSER_UNITDEVICE_ENUM_ENTRY pBaseInfo);
	UINT32 UnitDevicesCount() { return m_mapUnitDevices.size(); }
	BOOL UnitDevicesInitialize();
	void ClearUnitDevices();

	WTL::CString GetName();

	CNBUnitDevice * operator[](int i) { return m_mapUnitDevices.count(i) ? m_mapUnitDevices[i] : NULL; }
};

class CNBDevice
{
public:
	WTL::CString GetCapacityString();
	static WTL::CString CNBDevice::GetCapacityString(UINT64 ui64capacity);
	static UINT FindIconIndex(UINT idicon, UINT *anIconIDs, int nCount, int iDefault = 0);

	virtual UINT32 GetType() = 0;
	virtual WTL::CString GetTypeString() = 0;
	virtual WTL::CString GetName() = 0;
	virtual WTL::CString GetIDString() = 0;
	virtual UINT GetIconIndex(UINT *anIconIDs, int nCount) = 0;
	virtual UINT GetSelectIconIndex(UINT *anIconIDs, int nCount) = 0;
	virtual BOOL IsGroup() = 0;
	virtual BOOL IsHDD() = 0;
	virtual BOOL GetCommandAbility(int nID) = 0;
	virtual UINT64 GetCapacityInByte() = 0;
	virtual WTL::CString GetStatusString() = 0;
	virtual BOOL IsFaultTolerant() = 0;
	virtual WTL::CString GetFaultToleranceString() = 0;
	virtual BOOL IsOperatable() = 0;
	virtual DWORD GetAccessMask() = 0;
	virtual BOOL HixChangeNotify(LPCGUID guid) = 0;
};


// 1 per 1 NDAS Unit Device
class CNBUnitDevice : public CNBDevice
{
	friend class CNBNdasDevice;
	friend class CNBLogicalDevice;
private:
	NDASUSER_UNITDEVICE_ENUM_ENTRY m_BaseInfo;

	CNBNdasDevice *m_pDevice;
	CNBLogicalDevice *m_pLogicalDevice;

	NDAS_DIB_V2 m_DIB; // NdasOpReadDIB;
	NDAS_RAID_META_DATA m_RMD; // NdasOpRMDRead
	UINT32 m_cSequenceInRMD;

public:

	CNBUnitDevice(PNDASUSER_UNITDEVICE_ENUM_ENTRY pBaseInfo);
	BOOL Initialize();
	BOOL InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess);

	virtual UINT32 GetType();
	virtual WTL::CString GetTypeString();
	virtual WTL::CString GetName();
	virtual WTL::CString GetIDString();
	virtual UINT GetIconIndex(UINT *anIconIDs, int nCount);
	virtual UINT GetSelectIconIndex(UINT *anIconIDs, int nCount);
	virtual BOOL IsGroup();
	virtual BOOL IsHDD();
	virtual BOOL GetCommandAbility(int nID);
	virtual UINT64 GetCapacityInByte();
	virtual WTL::CString GetStatusString();
	virtual BOOL IsFaultTolerant();
	virtual WTL::CString GetFaultToleranceString();
	virtual BOOL IsOperatable();
	virtual DWORD GetAccessMask();
	virtual BOOL HixChangeNotify(LPCGUID guid);

	// device specific
	DWORD GetStatus();
	DWORD GetFaultTolerance();
	UINT32 GetSequence();
	BOOL IsFault();
	BOOL IsSpare();
	BOOL IsSibling(CNBUnitDevice *pUnitDevice);
	CNBLogicalDevice *GetLogicalDevice() { return m_pLogicalDevice; }
};

// 1 per 1 Unit
class CNBLogicalDevice : public CNBDevice
{
	friend class CNBUnitDevice;
private:
	NBUnitDevicePtrMap m_mapUnitDevices;

	CNBUnitDevice *AnyUnitDevice();
public:
	CNBLogicalDevice();
	BOOL UnitDeviceAdd(CNBUnitDevice *pUnitDevice);
	BOOL IsMember(CNBUnitDevice *pUnitDevice);

	virtual UINT32 GetType();
	virtual WTL::CString GetTypeString();
	virtual WTL::CString GetName();
	virtual WTL::CString GetIDString();
	virtual UINT GetIconIndex(UINT *anIconIDs, int nCount);
	virtual UINT GetSelectIconIndex(UINT *anIconIDs, int nCount);
	virtual BOOL IsGroup();
	virtual BOOL IsHDD();
	virtual BOOL GetCommandAbility(int nID);
	virtual UINT64 GetCapacityInByte();
	virtual WTL::CString GetStatusString();
	virtual BOOL IsFaultTolerant();
	virtual DWORD GetFaultTolerance();
	virtual WTL::CString GetFaultToleranceString();
	virtual BOOL IsOperatable();
	virtual DWORD GetAccessMask();
	virtual BOOL HixChangeNotify(LPCGUID guid);

	// device specific
	CNBUnitDevice *UnitDeviceInRMD(UINT32 nIndex);
	BOOL IsOperatableAll();
	UINT32 DevicesTotal(BOOL bAliveOnly = FALSE);
	UINT32 DevicesInRaid(BOOL bAliveOnly = FALSE);
	UINT32 DevicesSpare(BOOL bAliveOnly = FALSE);
	DWORD GetStatus();
	NBUnitDevicePtrList GetOperatableDevices();
	NDAS_DIB_V2 *DIB() { return &AnyUnitDevice()->m_DIB; }
	NDAS_RAID_META_DATA *RMD() { return &AnyUnitDevice()->m_RMD; }
	CNBUnitDevice * operator[](int i) { return m_mapUnitDevices.count(i) ? m_mapUnitDevices[i] : NULL; }
};