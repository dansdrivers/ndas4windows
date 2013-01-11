#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <ndas/ndastypeex.h>
#include "ndassvcdef.h"
#include "ndashostinfocache.h"

template <typename Pair>
struct select1st {
	typedef Pair argument_type;
	typedef typename Pair::first_type result_type;
	const result_type& operator()(const argument_type& p) const {
		return p.first;
	}
};

template <typename Pair>
struct select2nd {
	typedef Pair argument_type;
	typedef typename Pair::second_type result_type;
	const result_type& operator()(const argument_type& p) const {
		return p.second;
	}
};

template <typename T>
struct weak_ptr_to_shared_ptr : 
	public std::unary_function<boost::weak_ptr<T>,boost::shared_ptr<T> > {
	result_type operator ()(const argument_type& _arg) const {
		result_type _result(_arg);
		return _result;
	}
};


class CNdasDeviceRegistrar;
class CNdasLogicalDeviceManager;
class CNdasEventPublisher;
class CNdasEventMonitor;
class CNdasDeviceHeartbeatListener;
class CNdasServicePowerEventHandler;
class CNdasServiceDeviceEventHandler;

CNdasDeviceRegistrar&           pGetNdasDeviceRegistrar();
CNdasLogicalDeviceManager&      pGetNdasLogicalDeviceManager();
CNdasEventPublisher&            pGetNdasEventPublisher();
CNdasEventMonitor&              pGetNdasEventMonitor();
CNdasDeviceHeartbeatListener&   pGetNdasDeviceHeartbeatListener();
CNdasServicePowerEventHandler&  pGetNdasPowerEventHandler();
CNdasServiceDeviceEventHandler& pGetNdasDeviceEventHandler();

CNdasDevicePtr pGetNdasDevice(DWORD SlotNo);
CNdasDevicePtr pGetNdasDevice(const NDAS_DEVICE_ID& deviceId);
CNdasDevicePtr pGetNdasDevice(const NDAS_DEVICE_ID_EX& device);

CNdasUnitDevicePtr pGetNdasUnitDevice(const NDAS_DEVICE_ID& deviceId, DWORD UnitNo);
CNdasUnitDevicePtr pGetNdasUnitDevice(const NDAS_UNITDEVICE_ID& unitDeviceId);
CNdasUnitDevicePtr pGetNdasUnitDevice(DWORD SlotNo, DWORD UnitNo);
CNdasUnitDevicePtr pGetNdasUnitDevice(const NDAS_DEVICE_ID_EX& device, DWORD unitNo);

CNdasLogicalDevicePtr pGetNdasLogicalDevice(const NDAS_UNITDEVICE_ID& unitDeviceId);
CNdasLogicalDevicePtr pGetNdasLogicalDeviceByNdasLocation(NDAS_LOCATION Location);
CNdasLogicalDevicePtr pGetNdasLogicalDevice(NDAS_LOGICALDEVICE_ID logDevId);

LPCGUID pGetNdasHostGuid();
CNdasHostInfoCache* pGetNdasHostInfoCache();

