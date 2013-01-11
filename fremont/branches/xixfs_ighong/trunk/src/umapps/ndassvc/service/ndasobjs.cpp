#include "stdafx.h"
#include "ndascfg.h"
#include "ndasobjs.h"
#include "objbase.h"
#include "ndasdevreg.h"
#include "ndaslogdevman.h"

CNdasDeviceRegistrar&
pGetNdasDeviceRegistrar()
{
	return CNdasService::Instance()->GetDeviceRegistrar();
}

CNdasLogicalDeviceManager&
pGetNdasLogicalDeviceManager()
{
	return CNdasService::Instance()->GetLogicalDeviceManager();
}

CNdasDevicePtr
pGetNdasDevice(DWORD SlotNo)
{
	return pGetNdasDeviceRegistrar().Find(SlotNo);
}

CNdasDevicePtr 
pGetNdasDevice(const NDAS_DEVICE_ID& deviceId)
{
	return pGetNdasDeviceRegistrar().Find(deviceId);
}

CNdasDevicePtr
pGetNdasDevice(const NDAS_DEVICE_ID_EX& device)
{
	return pGetNdasDeviceRegistrar().Find(device);
}

CNdasUnitDevicePtr 
pGetNdasUnitDevice(const NDAS_DEVICE_ID& deviceId, DWORD UnitNo)
{
	CNdasDevicePtr pDevice = pGetNdasDevice(deviceId);
	if (CNdasDeviceNullPtr == pDevice) 
	{
		return CNdasUnitDeviceNullPtr;
	}
	return pDevice->GetUnitDevice(UnitNo);
}

CNdasUnitDevicePtr
pGetNdasUnitDevice(const NDAS_UNITDEVICE_ID& unitDeviceId)
{
	return pGetNdasUnitDevice(unitDeviceId.DeviceId, unitDeviceId.UnitNo);
}

CNdasUnitDevicePtr 
pGetNdasUnitDevice(DWORD SlotNo, DWORD UnitNo)
{
	CNdasDevicePtr pDevice = pGetNdasDevice(SlotNo);
	if (CNdasDeviceNullPtr == pDevice) 
	{
		return CNdasUnitDeviceNullPtr;
	}
	return pDevice->GetUnitDevice(UnitNo);
}

CNdasUnitDevicePtr
pGetNdasUnitDevice(const NDAS_DEVICE_ID_EX& device, DWORD unitNo)
{
	CNdasDevicePtr pDevice = pGetNdasDevice(device);
	if (CNdasDeviceNullPtr == pDevice) 
	{
		return CNdasUnitDeviceNullPtr;
	}
	return pDevice->GetUnitDevice(unitNo);
}

CNdasLogicalDevicePtr
pGetNdasLogicalDevice(CONST NDAS_UNITDEVICE_ID& unitDeviceId)
{
	CNdasUnitDevicePtr pUnitDevice = pGetNdasUnitDevice(unitDeviceId);
	if (CNdasUnitDeviceNullPtr == pUnitDevice) 
	{
		return CNdasLogicalDeviceNullPtr;
	}
	return pUnitDevice->GetLogicalDevice();
}

CNdasLogicalDevicePtr
pGetNdasLogicalDevice(CONST NDAS_SCSI_LOCATION& scsiLocation)
{
	CNdasLogicalDeviceManager& manager = 
		CNdasService::Instance()->GetLogicalDeviceManager();
	return manager.Find(scsiLocation);
}

CNdasLogicalDevicePtr
pGetNdasLogicalDevice(NDAS_LOGICALDEVICE_ID logDevId)
{
	CNdasLogicalDeviceManager& manager = 
		CNdasService::Instance()->GetLogicalDeviceManager();
	return manager.Find(logDevId);
}

CNdasEventPublisher&
pGetNdasEventPublisher()
{
	return CNdasService::Instance()->GetEventPublisher();
}

CNdasEventMonitor&
pGetNdasEventMonitor()
{
	return CNdasService::Instance()->GetEventMonitor();
}

CNdasDeviceHeartbeatListener&
pGetNdasDeviceHeartbeatListener()
{
	return CNdasService::Instance()->GetDeviceHeartbeatListener();
}

//CNdasServicePowerEventHandler&
//pGetNdasPowerEventHandler()
//{
//	return CNdasService::Instance()->Get
//}

CNdasServiceDeviceEventHandler&
pGetNdasDeviceEventHandler()
{
	return CNdasService::Instance()->GetDeviceEventHandler();
}

LPCGUID
pGetNdasHostGuid()
{
	static LPGUID pHostGuid = NULL;
	static GUID hostGuid;

	if (NULL != pHostGuid) 
	{
		return pHostGuid;
	}

	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		_T("Host"),
		_T("HostID"),
		&hostGuid,
		sizeof(GUID));

	if (!fSuccess) {
		HRESULT hr = ::CoCreateGuid(&hostGuid);
		XTLASSERT(SUCCEEDED(hr));

		fSuccess = _NdasSystemCfg.SetValueEx(
			_T("Host"),
			_T("HostID"),
			REG_BINARY,
			&hostGuid,
			sizeof(GUID));

	}

	pHostGuid = &hostGuid;
	return pHostGuid;
}

CNdasHostInfoCache*
pGetNdasHostInfoCache()
{
	static CNdasHostInfoCache* phi = NULL;
	if (NULL != phi) {
		return phi;
	}
	phi = new CNdasHostInfoCache();
	XTLASSERT(NULL != phi);
	return phi;
}

