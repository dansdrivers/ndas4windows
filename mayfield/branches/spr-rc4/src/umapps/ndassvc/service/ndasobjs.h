#pragma once
#include "ndas/ndastypeex.h"
#include "ndasinstman.h"
#include "ndasdev.h"
#include "ndasunitdev.h"
#include "ndasdevreg.h"
#include "ndaslogdev.h"
#include "ndaslogdevman.h"
#include "ndashostinfocache.h"

//
// SmartPointer for RefObj
// This only releases Reference Counter when 
// it goes out of scope.
// 
template <typename T>
class CRefObjPtr
{
public:
	T* p;
	CRefObjPtr() throw() : p(NULL) {}
	CRefObjPtr(int nNull) throw() : p(NULL)
	{
		_ASSERTE(0 == nNull); (void) nNull;
	}
	CRefObjPtr(T* lp) throw() : p(lp) {} //  we do not add-ref here
	~CRefObjPtr() {	if (p) p->Release(); }
	T* operator ->() const throw()
	{
		_ASSERTE(NULL != p);
		return (T*)p;
	}
	operator T*() const throw()
	{
		return p;
	}
	T& operator*() const throw()
	{
		_ASSERTE(p!=NULL);
		return *p;
	}
	bool operator==(T* pT) const throw()
	{
		return p == pT;
	}
	// Attach to an existing interface (does not AddRef)
	void Attach(T* p2) throw()
	{
		if (p) p->Release();
		p = p2;
	}
	// Detach the interface (does not Release)
	T* Detach() throw()
	{
		T* pt = p; p = NULL; return pt;
	}
private:
	// Copy constructor is prohibited.
	CRefObjPtr(const CRefObjPtr&);
	// Assignment operation is prohibited.
	CRefObjPtr& operator = (const CRefObjPtr&);
};

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
	CRefObjPtr<CNdasDevice> pDevice = pGetNdasDevice(deviceId);
	if (NULL == pDevice.p) {
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
	CRefObjPtr<CNdasDevice> pDevice = pGetNdasDevice(SlotNo);
	if (NULL == pDevice.p) {
		return NULL;
	}
	return pDevice->GetUnitDevice(UnitNo);
}

inline CNdasLogicalDevice* 
pGetNdasLogicalDevice(CONST NDAS_UNITDEVICE_ID& unitDeviceId)
{
	CRefObjPtr<CNdasUnitDevice> pUnitDevice = pGetNdasUnitDevice(unitDeviceId);
	if (NULL == pUnitDevice.p) {
		return NULL;
	}
	return pUnitDevice->GetLogicalDevice();
}

inline CNdasLogicalDevice*
pGetNdasLogicalDevice(CONST NDAS_SCSI_LOCATION& scsiLocation)
{
	CNdasLogicalDeviceManager* pLdm = 
		pGetNdasLogicalDeviceManager();
	return pLdm->Find(scsiLocation);
}

inline CNdasLogicalDevice* 
pGetNdasLogicalDevice(NDAS_LOGICALDEVICE_ID logDevId)
{
	CNdasLogicalDeviceManager* pLdm =
		pGetNdasLogicalDeviceManager();
	return pLdm->Find(logDevId);
}

inline CNdasInstanceManager*
pGetNdasInstanceManager()
{
	CNdasInstanceManager* pInstMan = 
		CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);
	return pInstMan;
}

inline CNdasEventPublisher*
pGetNdasEventPublisher()
{
	CNdasEventPublisher* p = 
		pGetNdasInstanceManager()->GetEventPublisher();
	_ASSERTE(NULL != p);
	return p;
}

inline CNdasEventMonitor*
pGetNdasEventMonitor()
{
	CNdasEventMonitor* p = 
		pGetNdasInstanceManager()->GetEventMonitor();
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

