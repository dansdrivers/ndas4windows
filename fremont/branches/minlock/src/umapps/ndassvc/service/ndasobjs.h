#pragma once
#include <functional>
#include <ndas/ndastypeex.h>
#include "ndassvcdef.h"
#include "ndashostinfocache.h"

/* class for the compose_f_gx adapter
*/
template <class OP1, class OP2>
class compose1_t
	: public std::unary_function<typename OP2::argument_type,
	typename OP1::result_type>
{
private:
	OP1 op1;    // process: op1(op2(x))
	OP2 op2;
public:
	// constructor
	compose1_t(const OP1& o1, const OP2& o2)
		: op1(o1), op2(o2) {
	}

	// function call
	typename OP1::result_type
		operator()(const typename OP2::argument_type& x) const {
			return op1(op2(x));
	}
};

/* convenience function for the compose_f_gx adapter
*/
template <class OP1, class OP2>
inline compose1_t<OP1,OP2>
compose1 (const OP1& o1, const OP2& o2) {
	return compose1_t<OP1,OP2>(o1,o2);
}

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
struct RawPointerToComPtr : public std::unary_function<T*,CComPtr<T> >
{
	result_type operator()(argument_type _arg) const
	{
		result_type _result(_arg);
		return _result;
	}
};

class CNdasEventPublisher;
class CNdasEventMonitor;
class CNdasDeviceHeartbeatListener;
class CNdasServicePowerEventHandler;
class CNdasServiceDeviceEventHandler;

CNdasEventPublisher&            pGetNdasEventPublisher();
CNdasEventMonitor&              pGetNdasEventMonitor();
CNdasDeviceHeartbeatListener&   pGetNdasDeviceHeartbeatListener();
CNdasServicePowerEventHandler&  pGetNdasPowerEventHandler();
CNdasServiceDeviceEventHandler& pGetNdasDeviceEventHandler();

HRESULT pGetNdasDeviceRegistrar(__deref_out INdasDeviceRegistrar** ppNdasDeviceRegistrar);
HRESULT pGetNdasLogicalUnitManager(__deref_out INdasLogicalUnitManager** ppManager);

HRESULT pGetNdasLogicalUnitManagerInternal(__deref_out INdasLogicalUnitManagerInternal** ppManager);

HRESULT pGetNdasDevice(DWORD SlotNo, INdasDevice** ppNdasDevice);
HRESULT pGetNdasDevice(const NDAS_DEVICE_ID& deviceId, INdasDevice** ppNdasDevice);
HRESULT pGetNdasDevice(const NDAS_DEVICE_ID_EX& device, INdasDevice** ppNdasDevice);

HRESULT pGetNdasUnit(const NDAS_DEVICE_ID& deviceId, DWORD UnitNo, INdasUnit** ppNdasUnit);
HRESULT pGetNdasUnit(const NDAS_UNITDEVICE_ID& unitDeviceId, INdasUnit** ppNdasUnit);
HRESULT pGetNdasUnit(DWORD SlotNo, DWORD UnitNo, INdasUnit** ppNdasUnit);
HRESULT pGetNdasUnit(const NDAS_DEVICE_ID_EX& device, DWORD unitNo, INdasUnit** ppNdasUnit);

HRESULT pGetNdasLogicalUnit(const NDAS_UNITDEVICE_ID& unitDeviceId, INdasLogicalUnit** ppNdasLogicalUnit);
HRESULT pGetNdasLogicalUnitByNdasLocation(NDAS_LOCATION Location, INdasLogicalUnit** ppNdasLogicalUnit);
HRESULT pGetNdasLogicalUnit(NDAS_LOGICALDEVICE_ID logDevId, INdasLogicalUnit** ppNdasLogicalUnit);

LPCGUID pGetNdasHostGuid();
CNdasHostInfoCache* pGetNdasHostInfoCache();

DWORD
pReadMaxRequestBlockLimitConfig(DWORD hardwareVersion);	

DWORD
pGetNdasUserId(DWORD UnitNo, ACCESS_MASK Access);
