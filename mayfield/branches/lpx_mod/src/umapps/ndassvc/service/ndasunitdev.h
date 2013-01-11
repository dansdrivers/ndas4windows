#pragma once
#include "extobj.h"
#include "syncobj.h"
#include "ndas/ndastypeex.h"
#include "ndas/ndascntenc.h"
#include "objstr.h"

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
	public ximeta::CCritSecLock,
	public ximeta::CExtensibleObject,
	public ximeta::CStringizer<CNdasUnitDevice,32>
{
	friend class CNdasDevice;
	friend class CNdasLogicalDevice;
	friend class CNdasUnitDeviceCreator;

protected:

	//
	// An instance of the NDAS device containing 
	// this unit device
	//
	CNdasDevice* CONST m_pParentDevice;
	CNdasLogicalDevice* m_pLogicalDevice;

	//
	// Unit Device ID
	//
	CONST NDAS_UNITDEVICE_ID m_unitDeviceId;
	CONST NDAS_UNITDEVICE_TYPE m_type;
	CONST NDAS_UNITDEVICE_SUBTYPE m_subType;
	CONST NDAS_LOGICALDEVICE_GROUP m_ldGroup;
	CONST DWORD m_ldSequence;

	NDAS_UNITDEVICE_INFORMATION m_devInfo;
	NDAS_UNITDEVICE_STATUS m_status;
	NDAS_UNITDEVICE_ERROR m_lastError;
	NDAS_UNITDEVICE_PRIMARY_HOST_INFO m_PrimaryHostInfo;

	TCHAR m_szRegContainer[30];

public:

	virtual ULONG AddRef();
	virtual ULONG Release();

public:

	CNdasUnitDevice(
		CNdasDevice& parentDevice, 
		DWORD dwUnitNo, 
		NDAS_UNITDEVICE_TYPE m_type,
		NDAS_UNITDEVICE_SUBTYPE m_subType,
		CONST NDAS_UNITDEVICE_INFORMATION& unitDevInfo,
		CONST NDAS_LOGICALDEVICE_GROUP& ldGroup,
		DWORD ldSequence);

	virtual ~CNdasUnitDevice();

	DWORD GetLDSequence();
	CONST NDAS_LOGICALDEVICE_GROUP& GetLDGroup();

	virtual BOOL RegisterToLDM();
	virtual BOOL UnregisterFromLDM();

	virtual VOID SetStatus(NDAS_UNITDEVICE_STATUS newStatus);
	virtual VOID SetHostUsageCount(DWORD nROHosts, DWORD nRWHosts);

	CONST NDAS_UNITDEVICE_ID& GetUnitDeviceId();
	
	virtual NDAS_UNITDEVICE_TYPE GetType();
	virtual NDAS_UNITDEVICE_SUBTYPE GetSubType();
	virtual NDAS_UNITDEVICE_STATUS GetStatus();
	virtual NDAS_UNITDEVICE_ERROR GetLastError();
	virtual BOOL SetMountStatus(BOOL bMounted);

	virtual CONST NDAS_UNITDEVICE_INFORMATION& GetUnitDevInfo();

	//
	// access mask of the configuration 
	//
	virtual ACCESS_MASK GetGrantedAccess();

	//
	// access mask of the running configuration
	//
	virtual ACCESS_MASK GetAllowingAccess();

	virtual CNdasDevice* GetParentDevice();
	virtual DWORD GetUnitNo();

	virtual BOOL GetHostUsageCount(
		LPDWORD lpnROHosts, 
		LPDWORD lpnRWHosts, 
		BOOL bUpdate = FALSE);

	virtual BOOL GetActualHostUsageCount(
		LPDWORD lpnROHosts,
		LPDWORD lpnRWHosts,
		BOOL bUpdate = FALSE);

	virtual ULONG GetUserBlockCount();
	virtual ULONG GetPhysicalBlockCount();

	virtual VOID UpdatePrimaryHostInfo(
		CONST NDAS_UNITDEVICE_PRIMARY_HOST_INFO& info);

	virtual BOOL CheckNDFSCompatibility();
	virtual DWORD GetOptimalMaxRequestBlock();

	virtual CNdasLogicalDevice* GetLogicalDevice();

	virtual LPCTSTR ToString();

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
	ULONG m_ulUserBlocks;
	NDAS_DIB_V2* m_pDIBv2;
	NDAS_CONTENT_ENCRYPT m_contentEncrypt;

	//virtual VOID ProcessDiskInfoBlock_(PNDAS_DIB_V2 pDib);

	//
	// Constructor
	//
	CNdasUnitDiskDevice(
		CNdasDevice& parentDevice, 
		DWORD dwUnitNo, 
		NDAS_UNITDEVICE_DISK_TYPE diskType,
		CONST NDAS_UNITDEVICE_INFORMATION& unitDevInfo,
		CONST NDAS_LOGICALDEVICE_GROUP& ldGroup,
		DWORD ldSequence,
		ULONG ulUserBlocks,
		PVOID pAddTargetInfo,
		CONST NDAS_CONTENT_ENCRYPT& ce,
		NDAS_DIB_V2* pDIBv2);

public:

	//
	// Destructor
	//
	virtual ~CNdasUnitDiskDevice();

	virtual ULONG GetUserBlockCount();
	virtual PVOID GetAddTargetInfo();
	virtual BOOL IsBitmapClean();
	virtual CONST NDAS_CONTENT_ENCRYPT& GetEncryption();
	virtual BOOL HasSameDIBInfo();
	virtual BOOL HasSameDIBInfo(CNdasUnitDiskDevice &NdasUnitDiskDevice);

private:

	// hide copy constructor
	CNdasUnitDiskDevice(const CNdasUnitDevice &);
	// hide assignment operator
	CNdasUnitDiskDevice& operator = (const CNdasUnitDiskDevice&);

};
