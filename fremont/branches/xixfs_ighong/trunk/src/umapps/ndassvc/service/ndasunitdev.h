#pragma once
#include <xtl/xtltrace.h>
#include "ndas/ndastypeex.h"
#include "ndas/ndascntenc.h"
#include "ndassvcdef.h"
#include "objstr.h"
#include "ndasobjs.h"
#include "syncobj.h"

class CNdasDeviceComm;
class CNdasDevice;
class CNdasLogicalDevice;
class CNdasUnitDevice;

//////////////////////////////////////////////////////////////////////////
//
// Unit Device Class
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitDevice :
	public ximeta::CCritSecLockGlobal,
	public CStringizer,
	public boost::enable_shared_from_this<CNdasUnitDevice>
{
	// friend class CNdasDevice;
	// friend class CNdasLogicalDevice;
	friend class CNdasUnitDeviceCreator;

protected:

	//
	// An instance of the NDAS device containing 
	// this unit device
	//
	CNdasDeviceWeakPtr m_pParentDevice;
	CNdasLogicalDevicePtr m_pLogicalDevice;

	//
	// Unit Device ID
	//
	const NDAS_UNITDEVICE_ID m_unitDeviceId;
	const NDAS_UNITDEVICE_TYPE m_type;
	const NDAS_UNITDEVICE_SUBTYPE m_subType;
	const NDAS_LOGICALDEVICE_GROUP m_ldGroup;
	const DWORD m_ldSequence;

	NDAS_UNITDEVICE_HARDWARE_INFO m_udinfo;
	NDAS_UNITDEVICE_STATUS m_status;
	NDAS_UNITDEVICE_ERROR m_lastError;
	NDAS_UNITDEVICE_PRIMARY_HOST_INFO m_PrimaryHostInfo;

	bool m_bSupposeFault;

	TCHAR m_szRegContainer[30];

public:

	typedef ximeta::CAutoLock<CNdasUnitDevice> InstanceAutoLock;

	CNdasUnitDevice(
		CNdasDevicePtr pParentDevice, 
		DWORD UnitNo, 
		NDAS_UNITDEVICE_TYPE m_type,
		NDAS_UNITDEVICE_SUBTYPE m_subType,
		const NDAS_UNITDEVICE_HARDWARE_INFO& unitDevInfo,
		const NDAS_LOGICALDEVICE_GROUP& ldGroup,
		DWORD ldSequence);

	virtual ~CNdasUnitDevice();

	CNdasDevicePtr GetParentDevice() const;
	const NDAS_UNITDEVICE_ID& GetUnitDeviceId() const;
	void GetUnitDeviceId(NDAS_UNITDEVICE_ID* pUnitDeviceId) const;
	DWORD GetUnitNo() const;
	bool IsVolatile() const;

	virtual DWORD GetLDSequence() const;
	virtual const NDAS_LOGICALDEVICE_GROUP& GetLDGroup() const;
	virtual NDAS_UNITDEVICE_TYPE GetType() const;
	virtual NDAS_UNITDEVICE_SUBTYPE GetSubType() const;

	virtual UINT32 GetBACLSize(int nBACLSkipped = 0) const;
	virtual BOOL FillBACL(void *pNdasBlockAcl) const;

	virtual bool RegisterToLogicalDeviceManager();
	virtual bool UnregisterFromLogicalDeviceManager();

	// virtual void SetHostUsageCount(DWORD nROHosts, DWORD nRWHosts);

	virtual NDAS_UNITDEVICE_STATUS GetStatus();
	virtual NDAS_UNITDEVICE_ERROR GetLastError();

	virtual const NDAS_UNITDEVICE_HARDWARE_INFO& GetHardwareInfo();
	virtual void GetHardwareInfo(PNDAS_UNITDEVICE_HARDWARE_INFO pudinfo);

	virtual BOOL UpdateStats();
	virtual const NDAS_UNITDEVICE_STAT& GetStats();
	virtual void GetStats(NDAS_UNITDEVICE_STAT& stat);

	//
	// access mask of the configuration 
	//
	virtual ACCESS_MASK GetGrantedAccess();

	//
	// access mask of the running configuration
	//
	virtual ACCESS_MASK GetAllowingAccess();


	virtual BOOL GetHostUsageCount(
		LPDWORD lpnROHosts, 
		LPDWORD lpnRWHosts, 
		BOOL bUpdate = FALSE);

	virtual BOOL GetActualHostUsageCount(
		LPDWORD lpnROHosts,
		LPDWORD lpnRWHosts,
		BOOL bUpdate = FALSE);

	virtual UINT64 GetUserBlockCount();
	virtual UINT64 GetPhysicalBlockCount();

	virtual void UpdatePrimaryHostInfo(
		const NDAS_UNITDEVICE_PRIMARY_HOST_INFO& info);

	virtual BOOL CheckNDFSCompatibility();
	virtual DWORD GetOptimalMaxRequestBlock();

	virtual CNdasLogicalDevicePtr GetLogicalDevice();

	UINT64 GetDevicePassword();
	DWORD GetDeviceUserID(ACCESS_MASK access);

	void SetFault(bool bFault = true);
	bool IsFault();

	static NDAS_UNITDEVICE_SUBTYPE 
	CreateSubType(WORD t)
	{
		NDAS_UNITDEVICE_SUBTYPE subType = {t};
		return subType;
	}

	void OnUnmounted();
	void OnMounted();

protected:

	virtual void _SetStatus(NDAS_UNITDEVICE_STATUS newStatus);

private:

	// hide copy constructor
	CNdasUnitDevice(const CNdasUnitDevice &);
	// hide assignment operator
	CNdasUnitDevice& operator = (const CNdasUnitDevice&);
};

//////////////////////////////////////////////////////////////////////////
//
// Unit Disk Device class
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitDiskDevice;

class CNdasUnitDiskDevice :
	public CNdasUnitDevice
{

	friend class CNdasUnitDeviceCreator;

protected:

	//
	// Unit disk type
	//
	NDAS_UNITDEVICE_DISK_TYPE m_diskType;
	PVOID m_pAddTargetInfo;
	UINT64 m_ulUserBlocks;
	NDAS_DIB_V2* m_pDIBv2;
	PBLOCK_ACCESS_CONTROL_LIST m_pBACL;
	NDAS_CONTENT_ENCRYPT m_contentEncrypt;

	//virtual void ProcessDiskInfoBlock_(PNDAS_DIB_V2 pDib);

	//
	// Constructor
	//
	CNdasUnitDiskDevice(
		CNdasDevicePtr parentDevice, 
		DWORD UnitNo, 
		NDAS_UNITDEVICE_DISK_TYPE diskType,
		const NDAS_UNITDEVICE_HARDWARE_INFO& unitDevInfo,
		const NDAS_LOGICALDEVICE_GROUP& ldGroup,
		DWORD ldSequence,
		ULONG ulUserBlocks,
		PVOID pAddTargetInfo,
		const NDAS_CONTENT_ENCRYPT& ce,
		NDAS_DIB_V2* pDIBv2,
		BLOCK_ACCESS_CONTROL_LIST *pBACL);

public:

	//
	// Destructor
	//
	virtual ~CNdasUnitDiskDevice();

	virtual UINT64 GetUserBlockCount();
	virtual PVOID GetAddTargetInfo();
	virtual BOOL IsBitmapClean();
	virtual const NDAS_CONTENT_ENCRYPT& GetEncryption();
	virtual BOOL HasSameDIBInfo();
	virtual BOOL HasSameDIBInfo(CNdasUnitDiskDevice &NdasUnitDiskDevice);
	virtual UINT32 GetBACLSize(int nBACLSkipped = 0) const;
	virtual BOOL FillBACL(void *pNdasBlockAcl) const;

private:

	// hide copy constructor
	CNdasUnitDiskDevice(const CNdasUnitDevice &);
	// hide assignment operator
	CNdasUnitDiskDevice& operator = (const CNdasUnitDiskDevice&);

};

const NDAS_UNITDEVICE_HARDWARE_INFO NDAS_UNITDEVICE_HARDWARE_INFO_NONE = {
	static_cast<DWORD>(sizeof(NDAS_UNITDEVICE_HARDWARE_INFO))
};

const NDAS_LOGICALDEVICE_GROUP NDAS_LOGICALDEVICE_GROUP_NONE = {0}; 
const NDAS_UNITDEVICE_SUBTYPE NDAS_UNITDEVICE_SUBTYPE_NONE = {0};

class CNdasNullUnitDiskDevice : 
	public CNdasUnitDiskDevice
{
public:
	CNdasNullUnitDiskDevice(
		CNdasDevicePtr pParentDevice,
		DWORD UnitNo,
		const NDAS_UNITDEVICE_HARDWARE_INFO& UnitDevHardwareInfo,
		NDAS_UNITDEVICE_ERROR Error);

	virtual ~CNdasNullUnitDiskDevice();

	virtual DWORD GetLDSequence() const;
	virtual const NDAS_LOGICALDEVICE_GROUP& GetLDGroup() const;
	virtual bool RegisterToLogicalDeviceManager();
	virtual bool UnregisterFromLogicalDeviceManager();
	virtual ACCESS_MASK GetAllowingAccess();
	virtual UINT64 GetUserBlockCount();
	virtual UINT64 GetPhysicalBlockCount();
	virtual BOOL CheckNDFSCompatibility();
	virtual CNdasLogicalDevicePtr GetLogicalDevice();
};

class CNdasNullUnitDevice : 
	public CNdasUnitDevice
{
public:
	CNdasNullUnitDevice(
		CNdasDevicePtr pParentDevice, 
		DWORD UnitNo);

	//CNdasNullUnitDevice(
	//	CNdasDevicePtr pParentDevice,
	//	DWORD UnitNo,
	//	NDAS_UNITDEVICE_TYPE Type,
	//	const NDAS_UNITDEVICE_HARDWARE_INFO& UnitDevHardwareInfo,
	//	NDAS_UNITDEVICE_ERROR Error);

	virtual ~CNdasNullUnitDevice();

	virtual DWORD GetLDSequence() const;
	virtual const NDAS_LOGICALDEVICE_GROUP& GetLDGroup() const;
	virtual bool RegisterToLogicalDeviceManager();
	virtual bool UnregisterFromLogicalDeviceManager();
	virtual ACCESS_MASK GetAllowingAccess();
	virtual UINT64 GetUserBlockCount();
	virtual UINT64 GetPhysicalBlockCount();
	virtual BOOL CheckNDFSCompatibility();
	virtual CNdasLogicalDevicePtr GetLogicalDevice();
};
