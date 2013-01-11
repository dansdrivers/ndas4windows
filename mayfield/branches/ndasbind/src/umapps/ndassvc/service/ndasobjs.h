#pragma once
#include "ndas/ndastypeex.h"
#include "ndasinstman.h"
#include "ndasdev.h"
#include "ndasunitdev.h"
#include "ndasdevreg.h"
#include "ndaslogdev.h"
#include "ndaslogdevman.h"
#include "ndashostinfocache.h"

inline CNdasDeviceRegistrar*
pGetNdasDeviceRegistrar()
{
	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasDeviceRegistrar* pRegistrar = pInstMan->GetRegistrar();
	_ASSERTE(NULL != pRegistrar);

	return pRegistrar;
}

inline CNdasLogicalDeviceManager* 
pGetNdasLogicalDeviceManager()
{
	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasLogicalDeviceManager* pLdm = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLdm);

	return pLdm;
}

inline CNdasDevice* 
pGetNdasDevice(DWORD SlotNo)
{
	return pGetNdasDeviceRegistrar()->Find(SlotNo);
}

inline CNdasDevice* 
pGetNdasDevice(const NDAS_DEVICE_ID& deviceId)
{
	return pGetNdasDeviceRegistrar()->Find(deviceId);
}

inline CNdasUnitDevice* 
pGetNdasUnitDevice(const NDAS_DEVICE_ID& deviceId, DWORD UnitNo)
{
	CNdasDevice* pDevice = pGetNdasDevice(deviceId);
	if (NULL == pDevice) {
		return NULL;
	}
	return pDevice->GetUnitDevice(UnitNo);
}

inline CNdasUnitDevice* 
pGetNdasUnitDevice(const NDAS_UNITDEVICE_ID& unitDeviceId)
{
	return pGetNdasUnitDevice(unitDeviceId.DeviceId, unitDeviceId.UnitNo);
}

inline CNdasUnitDevice* 
pGetNdasUnitDevice(DWORD SlotNo, DWORD UnitNo)
{
	CNdasDevice* pDevice = pGetNdasDevice(SlotNo);
	if (NULL == pDevice) {
		return NULL;
	}
	return pDevice->GetUnitDevice(UnitNo);
}

inline CNdasLogicalDevice* 
pGetNdasLogicalDevice(CONST NDAS_UNITDEVICE_ID& unitDeviceId)
{
	CNdasUnitDevice* pUnitDevice = pGetNdasUnitDevice(unitDeviceId);
	if (NULL == pUnitDevice) {
		return NULL;
	}
	return pUnitDevice->GetLogicalDevice();
}

inline CNdasLogicalDevice*
pGetNdasLogicalDevice(CONST NDAS_SCSI_LOCATION& scsiLocation)
{
	CNdasLogicalDeviceManager* pLdm = pGetNdasLogicalDeviceManager();
	return pLdm->Find(scsiLocation);
}

inline CNdasLogicalDevice* 
pGetNdasLogicalDevice(NDAS_LOGICALDEVICE_ID logDevId)
{
	CNdasLogicalDeviceManager* pLdm = pGetNdasLogicalDeviceManager();
	return pLdm->Find(logDevId);
}

inline CNdasInstanceManager*
pGetNdasInstanceManager()
{
	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);
	return pInstMan;
}

inline CNdasEventPublisher*
pGetNdasEventPublisher()
{
	CNdasEventPublisher* p = pGetNdasInstanceManager()->GetEventPublisher();
	_ASSERTE(NULL != p);
	return p;
}

inline CNdasEventMonitor*
pGetNdasEventMonitor()
{
	CNdasEventMonitor* p = pGetNdasInstanceManager()->GetEventMonitor();
	_ASSERTE(NULL != p);
	return p;
}

inline CNdasDeviceHeartbeatListener*
pGetNdasDeviceHeartbeatListner()
{
	CNdasDeviceHeartbeatListener* p =
		pGetNdasInstanceManager()->GetHBListener();
	_ASSERTE(NULL != p);
	return p;
}

inline CNdasServicePowerEventHandler*
pGetNdasPowerEventHandler()
{
	CNdasServicePowerEventHandler* p =
		pGetNdasInstanceManager()->GetPowerEventHandler();
	// This can be null
	// _ASSERTE(NULL != p);
	return p;
}

inline CNdasServiceDeviceEventHandler*
pGetNdasDeviceEventHandler()
{
	CNdasServiceDeviceEventHandler* p = 
		pGetNdasInstanceManager()->GetDeviceEventHandler();
	// This can be null
	// _ASSERTE(NULL != p);
	return p;
}

LPCGUID
pGetNdasHostGuid();

CNdasHostInfoCache*
pGetNdasHostInfoCache();
