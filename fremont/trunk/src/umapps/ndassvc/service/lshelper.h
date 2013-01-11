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
	public CStringizerA<32>,
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
	virtual NDAS_UN