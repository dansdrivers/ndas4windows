#pragma once

#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasdib.h>
#include <ndas/ndasop.h>
#include <ndas/ndasid.h>
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

#define NDASBIND_UNIT_DEVICE_RMD_INVALID	0x00000001
#define NDASBIND_UNIT_DEVICE_RMD_FAULT		0x00000002	// out of sync
#define NDASBIND_UNIT_DEVICE_RMD_SPARE		0x00000004
#define NDASBIND_UNIT_DEVICE_RMD_DEFECTIVE	0x00000008

#define NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED	0x00000001
#define NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY	0x00000002
#define NDASBIND_UNIT_DEVICE_STATUS_MOUNTED			0x00000004
#define NDASBIND_UNIT_DEVICE_STATUS_DISABLED		0x00000008
#define NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED   0x00000010
#define NDASBIND_UNIT_DEVICE_STATUS_UNKNOWN_RO_MOUNT 0x00000020

#define NDASBIND_LOGICAL_DEVICE_RMD_INVALID	0x00000001
#define NDASBIND_LOGICAL_DEVICE_RMD_FAULT	0x00000002
#define NDASBIND_LOGICAL_DEVICE_RMD_MISSING	0x00000004
#define NDASBIND_LOGICAL_DEVICE_RMD_BROKEN	0x00000008

#define NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED	0x00000001
#define NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY	0x00000002
#define NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED		0x00000004
#define NDASBIND_LOGICAL_DEVICE_STATUS_DISABLED		0x00000008
#define NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED 0x00000010
#define NDASBIND_LOGICAL_DEVICE_STATUS_UNKNOWN_MEDIA_TYPE 0x00000020

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
	NDASID_EXT_DATA m_IdExtData;
	NDAS_ID m_NdasId;

	BOOL			m_bServiceInfo; // TRUE if the information is retrieved from service
	BOOL			m_bVirtualDevice; // This NDAS device created based on other disk's meta data
public:
	CNBNdasDevice(PNDASUSER_DEVICE_ENUM_ENTRY pBaseInfo, NDAS_DEVICE_STATUS status);
	// Create NDAS device from scratch. Used to handle unregistered devices.
	CNBNdasDevice(LPCTSTR DeviceName, PNDAS_DEVICE_ID pDeviceId, NDAS_DEVICE_STATUS status);	
	CNBNdasDevice(CNBNdasDevice* SrcDev, BOOL CopyUnit);	// Create copy

	~CNBNdasDevice();

	// RaidFlags: ORed value of NDAS_RAID_MEMBER_FLAG_*
	CNBUnitDevice* AddUnitDevice(
		DWORD UnitNo,
		NDAS_UNITDEVICE_TYPE UnitDeviceType = NDAS_UNITDEVICE_TYPE_UNKNOWN,
		DWORD RaidFlags = 0);
	
	UINT32 UnitDevicesCount() { return m_mapUnitDevices.size(); }
	BOOL UnitDevicesInitialize();
	void ClearUnitDevices();

	BOOL IsAlive();

	virtual BOOL InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess)
	{
		return InitConnectionInfo(ci, bWriteAccess, 0);
	}

	BOOL InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess, DWORD UnitNo);

	CString GetName();

	CNBUnitDevice * operator[](int i) { return m_mapUnitDevices.count(i) ? m_mapUnitDevices[i] : NULL; }
	BOOL IsEqual(BYTE Node[6], BYTE Vid) {
		return !memcmp(
			m_DeviceId.Node, Node, 
			sizeof(m_DeviceId.Node))
			&& m_DeviceId.VID == Vid; }
};

class CNBDevice
{
public:
	static CString CNBDevice::GetCapacityString(UINT64 ui64capacity);
	static UINT FindIconIndex(UINT idicon, const UINT *anIconIDs, int nCount, int iDefault = 0);

	virtual CString GetCapacityString() =0;
	virtual UINT32 GetType() = 0;
	virtual CString GetRaidStatusString() = 0;
	virtual CString GetName() = 0;
	virtual CString GetIDString(TCHAR HiddenChar) = 0;
	virtual UINT GetIconIndex(const UINT *anIconIDs, int nCount) = 0;
	virtual UINT GetSelectIconIndex(const UINT *anIconIDs, int nCount) = 0;
	virtual BOOL IsGroup() = 0;
	virtual BOOL IsHDD() = 0;
	virtual BOOL IsCommandAvailable(int nID) = 0;
	virtual UINT64 GetLogicalCapacityInByte() = 0;
	virtual CString GetStatusString() = 0;
	virtual BOOL IsFaultTolerant() = 0;
	virtual CString GetCommentString() = 0;
	virtual BOOL IsOperatable() = 0;
	virtual DWORD GetAccessMask() = 0;
	virtual BOOL HixChangeNotify(LPCGUID guid) = 0;
	virtual BOOL InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess) = 0;
	virtual VOID UpdateStatus() = 0;
};


// 1 per 1 NDAS Unit Device
class CNBUnitDevice : public CNBDevice
{
	friend class CNBNdasDevice;
	friend class CNBLogicalDevice;
private:
	NDASUSER_UNITDEVICE_ENUM_ENTRY m_BaseInfo;

	CNBNdasDevice *m_pDevice;	// Can be NULL if unit device is not registered.

	CNBLogicalDevice *m_pLogicalDevice;

	NDAS_DIB_V2 m_DIB; // NdasOpReadDIB;
	NDAS_RAID_META_DATA m_RMD; // NdasOpRMDRead
	UINT32 m_cSequenceInRMD;

	DWORD			m_RaidFlags;	// ORed value of RAID_MEMBER_FLAG_*.
	BOOL			m_bMissingMember;	// This unit device is created to cover missing RAID member.
	DWORD			m_SequenceInDib;	// Determined when added logical device.
	UINT64			m_PhysicalCapacity;	// Set by Initialize.
	DWORD			m_Status;
	BOOL			m_bInitialized;
	
public:

	CNBUnitDevice(PNDASUSER_UNITDEVICE_ENUM_ENTRY pBaseInfo);
	void SetNotInitialized();
	BOOL Initialize(BOOL MissingInLogical = FALSE);

	virtual CString GetCapacityString();
	virtual UINT32 GetType();
	virtual CString GetRaidStatusString();
	virtual CString GetName();
	virtual CString GetIDString(TCHAR HiddenChar);
	virtual UINT GetIconIndex(const UINT *anIconIDs, int nCount);
	virtual UINT GetSelectIconIndex(const UINT *anIconIDs, int nCount);
	virtual BOOL IsGroup();
	virtual BOOL IsHDD();
	virtual BOOL IsCommandAvailable(int nID);
	virtual UINT64 GetLogicalCapacityInByte();
	virtual CString GetStatusString();
	virtual BOOL IsFaultTolerant();
	virtual CString GetCommentString();
	virtual BOOL IsOperatable();
	virtual DWORD GetAccessMask();
	virtual BOOL HixChangeNotify(LPCGUID guid);
	virtual BOOL InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess);
	virtual VOID UpdateStatus();

	// device specific
	UINT64 GetPhysicalCapacityInByte() { return m_PhysicalCapacity; }
	VOID SetRaidStatus(DWORD Status) { m_RaidFlags = Status;}
	DWORD GetRaidStatus() { return m_RaidFlags; }
	DWORD GetStatus();
	UINT32 GetSequence();
	BOOL IsDefective();
	BOOL IsNotSynced();
	BOOL IsSpare();
	BOOL IsMissingMember() { return m_bMissingMember; }
	CNBLogicalDevice *GetLogicalDevice() { return m_pLogicalDevice; }
	CNBNdasDevice *GetNdasDevice() { return m_pDevice; }	
	BOOL IsEqual(BYTE Node[6], BYTE Vid, DWORD UnitNo);
	BOOL IsThisDevice(BYTE Node[6], BYTE Vid);

};

// 1 per 1 Unit
class CNBLogicalDevice : public CNBDevice
{
	friend class CNBUnitDevice;
private:
	NBUnitDevicePtrMap m_mapUnitDevices;

	NDAS_DIB_V2 m_DIB;
	GUID	m_RaidSetId;
	GUID	m_ConfigSetId;
	CNBUnitDevice* m_PrimaryUnit;	// Metadata will be read based on this unit.
	NDASOP_RAID_INFO	m_RaidInfo;
	
public:
	CNBLogicalDevice();
	~CNBLogicalDevice();
	void UnitDeviceSet(CNBUnitDevice *pUnitDevice, UINT32 Sequence);
	BOOL IsMember(CNBUnitDevice *pUnitDevice);

	virtual CString GetCapacityString();
	virtual UINT32 GetType();
	virtual CString GetRaidStatusString();
	virtual CString GetName();
	virtual CString GetIDString(TCHAR HiddenChar);
	virtual UINT GetIconIndex(const UINT *anIconIDs, int nCount);
	virtual UINT GetSelectIconIndex(const UINT *anIconIDs, int nCount);
	virtual BOOL IsGroup();
	virtual BOOL IsHDD();
	virtual BOOL IsCommandAvailable(int nID);
	virtual UINT64 GetLogicalCapacityInByte();
	virtual CString GetStatusString();
	virtual BOOL IsFaultTolerant();
	virtual CString GetCommentString();
	virtual BOOL IsOperatable();
	virtual DWORD GetAccessMask();
	virtual BOOL HixChangeNotify(LPCGUID guid);
	virtual BOOL InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess);
	virtual VOID UpdateStatus();
	
	// device specific
	CNBUnitDevice *PrimaryUnitDevice();
	UINT32 GetOperatableCount();
	BOOL IsOperatableAll();
	BOOL HasMissingMember();
	BOOL IsHealthy();
	BOOL IsMigrationRequired();
	BOOL IsFixRaidStateRequired();
	UINT32 DevicesTotal(BOOL bAliveOnly = FALSE);
	UINT32 DevicesInRaid(BOOL bAliveOnly = FALSE);
	UINT32 DevicesSpare(BOOL bAliveOnly = FALSE);
	DWORD GetStatus();
	NBUnitDevicePtrList GetOperatableDevices();
	BOOL InitRaidInfo();

	NDAS_DIB_V2 *DIB() { return &PrimaryUnitDevice()->m_DIB; }
	NDAS_RAID_META_DATA *RMD() { return &PrimaryUnitDevice()->m_RMD; }
	NDASOP_RAID_INFO*	RAID_INFO() { return &m_RaidInfo; }
	CNBUnitDevice * operator[](int i) { return m_mapUnitDevices.count(i) ? m_mapUnitDevices[i] : NULL; }
};
