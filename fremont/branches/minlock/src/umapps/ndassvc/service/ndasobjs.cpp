#include "stdafx.h"
#include "ndascfg.h"
#include "ndasobjs.h"
#include "objbase.h"
#include "ndasdevid.h"
#include "ndasdevreg.h"

HRESULT
pGetNdasDeviceRegistrar(__deref_out INdasDeviceRegistrar** ppNdasDeviceRegistrar)
{
	return CNdasService::Instance()->GetDeviceRegistrar(ppNdasDeviceRegistrar);
}

HRESULT
pGetNdasLogicalUnitManager(__deref_out INdasLogicalUnitManager** ppManager)
{
	return CNdasService::Instance()->GetLogicalUnitManager(ppManager);
}

HRESULT pGetNdasLogicalUnitManagerInternal(__deref_out INdasLogicalUnitManagerInternal** ppManager)
{
	HRESULT hr;
	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));
	CComQIPtr<INdasLogicalUnitManagerInternal> pInternal(pManager);
	*ppManager = pInternal.Detach();
	return S_OK;
}


HRESULT
pGetNdasDevice(DWORD SlotNo, INdasDevice** ppNdasDevice)
{
	HRESULT hr;
	CComPtr<INdasDeviceRegistrar> pRegistrar;
	hr = pGetNdasDeviceRegistrar(&pRegistrar);
	if (SUCCEEDED(hr))
	{
		hr = pRegistrar->get_NdasDevice(SlotNo, ppNdasDevice);
	}
	return hr;
}

HRESULT 
pGetNdasDevice(const NDAS_DEVICE_ID& deviceId, INdasDevice** ppNdasDevice)
{
	HRESULT hr;
	CComPtr<INdasDeviceRegistrar> pRegistrar;
	hr = pGetNdasDeviceRegistrar(&pRegistrar);
	if (SUCCEEDED(hr))
	{
		hr = pRegistrar->get_NdasDevice(
			&const_cast<NDAS_DEVICE_ID&>(deviceId), ppNdasDevice);
	}
	return hr;
}

HRESULT
pGetNdasDevice(const NDAS_DEVICE_ID_EX& device, INdasDevice** ppNdasDevice)
{
	HRESULT hr;
	CComPtr<INdasDeviceRegistrar> pRegistrar;
	hr = pGetNdasDeviceRegistrar(&pRegistrar);
	if (SUCCEEDED(hr))
	{
		hr = pRegistrar->get_NdasDevice(
			&const_cast<NDAS_DEVICE_ID_EX&>(device), ppNdasDevice);
	}
	return hr;
}

HRESULT 
pGetNdasUnit(const NDAS_DEVICE_ID& deviceId, DWORD UnitNo, INdasUnit** ppNdasUnit)
{
	*ppNdasUnit = 0;
	CComPtr<INdasDevice> pNdasDevice;
	HRESULT hr = pGetNdasDevice(deviceId, &pNdasDevice);
	if (FAILED(hr)) 
	{
		return hr;
	}
	CComPtr<INdasUnit> pNdasUnit;
	hr = pNdasDevice->get_NdasUnit(UnitNo, &pNdasUnit);
	if (FAILED(hr))
	{
		return hr;
	}
	*ppNdasUnit = pNdasUnit.Detach();
	return S_OK;
}

HRESULT 
pGetNdasUnit(const NDAS_UNITDEVICE_ID& unitDeviceId, INdasUnit** ppNdasUnit)
{
	return pGetNdasUnit(unitDeviceId.DeviceId, unitDeviceId.UnitNo, ppNdasUnit);
}

HRESULT 
pGetNdasUnit(DWORD SlotNo, DWORD UnitNo, INdasUnit** ppNdasUnit)
{
	*ppNdasUnit = 0;
	CComPtr<INdasDevice> pNdasDevice;
	HRESULT hr = pGetNdasDevice(SlotNo, &pNdasDevice);
	if (FAILED(hr)) 
	{
		return hr;
	}
	CComPtr<INdasUnit> pNdasUnit;
	hr = pNdasDevice->get_NdasUnit(UnitNo, &pNdasUnit);
	if (FAILED(hr))
	{
		return hr;
	}
	*ppNdasUnit = pNdasUnit.Detach();
	return S_OK;
}

HRESULT 
pGetNdasUnit(const NDAS_DEVICE_ID_EX& device, DWORD unitNo, INdasUnit** ppNdasUnit)
{
	*ppNdasUnit = 0;
	CComPtr<INdasDevice> pNdasDevice;
	HRESULT hr = pGetNdasDevice(device, &pNdasDevice);
	if (FAILED(hr)) 
	{
		return hr;
	}
	CComPtr<INdasUnit> pNdasUnit;
	hr = pNdasDevice->get_NdasUnit(unitNo, &pNdasUnit);
	if (FAILED(hr))
	{
		return hr;
	}
	*ppNdasUnit = pNdasUnit.Detach();
	return S_OK;
}

HRESULT 
pGetNdasLogicalUnit(const NDAS_UNITDEVICE_ID& unitDeviceId, INdasLogicalUnit** ppNdasLogicalUnit)
{
	*ppNdasLogicalUnit = 0;
	CComPtr<INdasUnit> pNdasUnit;
	HRESULT hr = pGetNdasUnit(unitDeviceId, &pNdasUnit);
	if (FAILED(hr))
	{
		return hr;
	}
	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	hr = pNdasUnit->get_NdasLogicalUnit(&pNdasLogicalUnit);
	if (FAILED(hr))
	{
		return hr;
	}
	*ppNdasLogicalUnit = pNdasLogicalUnit.Detach();
	return S_OK;
}

HRESULT 
pGetNdasLogicalUnitByNdasLocation(NDAS_LOCATION Location, INdasLogicalUnit** ppNdasLogicalUnit)
{
	HRESULT hr;
	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));
	return pManager->get_NdasLogicalUnitByNdasLocation(Location, ppNdasLogicalUnit);
}

HRESULT 
pGetNdasLogicalUnit(NDAS_LOGICALDEVICE_ID logDevId, INdasLogicalUnit** ppNdasLogicalUnit)
{
	HRESULT hr;
	CComPtr<INdasLogicalUnitManager> pManager;
	COMVERIFY(hr = pGetNdasLogicalUnitManager(&pManager));
	return pManager->get_NdasLogicalUnit(logDevId, ppNdasLogicalUnit);
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

	BOOL success = _NdasSystemCfg.GetValueEx(
		_T("Host"),
		_T("HostID"),
		&hostGuid,
		sizeof(GUID));

	if (!success) {
		HRESULT hr = ::CoCreateGuid(&hostGuid);
		XTLASSERT(SUCCEEDED(hr));

		success = _NdasSystemCfg.SetValueEx(
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

