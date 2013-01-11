#pragma once
#include <dbt.h>
#include <pbt.h>
#include "xtldef.h"

namespace XTL
{

template <typename T>
class CDeviceEventHandler;

template <typename T>
class CDeviceEventHandler
{
public:

	//
	// Device Event Dispatcher
	//
	LRESULT DeviceEventProc(WPARAM wParam, LPARAM lParam);

protected:

	void OnConfigChangeCanceled() {}
	void OnConfigChanged() {}
	void OnDevNodesChanged() {}
	LRESULT OnQueryChangeConfig() { return TRUE; }

	void _OnCustomEvent(PDEV_BROADCAST_HDR hdr)
	{
		switch (hdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			static_cast<T*>(this)->OnDeviceInterfaceCustomEvent(
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(hdr));
			break;
		case DBT_DEVTYP_HANDLE:
			static_cast<T*>(this)->OnDeviceHandleCustomEvent(
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(hdr));
			break;
		case DBT_DEVTYP_OEM:
			static_cast<T*>(this)->OnOemCustomEvent(
				reinterpret_cast<PDEV_BROADCAST_OEM>(hdr));
			break;
		case DBT_DEVTYP_PORT:
			static_cast<T*>(this)->OnPortCustomEvent(
				reinterpret_cast<PDEV_BROADCAST_PORT>(hdr));
			break;
		case DBT_DEVTYP_VOLUME:
			static_cast<T*>(this)->OnVolumeCustomEvent(
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr));
			break;
		}
	}

	void _OnDeviceArrival(PDEV_BROADCAST_HDR hdr) 
	{
		switch (hdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			static_cast<T*>(this)->OnDeviceInterfaceArrival(
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(hdr));
			break;
		case DBT_DEVTYP_HANDLE:
			static_cast<T*>(this)->OnDeviceHandleArrival(
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(hdr));
			break;
		case DBT_DEVTYP_OEM:
			static_cast<T*>(this)->OnOemArrival(
				reinterpret_cast<PDEV_BROADCAST_OEM>(hdr));
			break;
		case DBT_DEVTYP_PORT:
			static_cast<T*>(this)->OnPortArrival(
				reinterpret_cast<PDEV_BROADCAST_PORT>(hdr));
			break;
		case DBT_DEVTYP_VOLUME:
			static_cast<T*>(this)->OnVolumeArrival(
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr));
			break;
		}
	}

	LRESULT _OnDeviceQueryRemove(PDEV_BROADCAST_HDR hdr) 
	{
		switch (hdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			return static_cast<T*>(this)->OnDeviceInterfaceQueryRemove(
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(hdr));
		case DBT_DEVTYP_HANDLE:
			return static_cast<T*>(this)->OnDeviceHandleQueryRemove(
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(hdr));
		case DBT_DEVTYP_OEM:
			return static_cast<T*>(this)->OnOemQueryRemove(
				reinterpret_cast<PDEV_BROADCAST_OEM>(hdr));
		case DBT_DEVTYP_PORT:
			return static_cast<T*>(this)->OnPortQueryRemove(
				reinterpret_cast<PDEV_BROADCAST_PORT>(hdr));
		case DBT_DEVTYP_VOLUME:
			return static_cast<T*>(this)->OnVolumeQueryRemove(
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr));
		default:
			return TRUE;
		}

	}

	void _OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR hdr)
	{
		switch (hdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			static_cast<T*>(this)->OnDeviceInterfaceQueryRemoveFailed(
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(hdr));
			break;
		case DBT_DEVTYP_HANDLE:
			static_cast<T*>(this)->OnDeviceHandleQueryRemoveFailed(
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(hdr));
			break;
		case DBT_DEVTYP_OEM:
			static_cast<T*>(this)->OnOemQueryRemoveFailed(
				reinterpret_cast<PDEV_BROADCAST_OEM>(hdr));
			break;
		case DBT_DEVTYP_PORT:
			static_cast<T*>(this)->OnPortQueryRemoveFailed(
				reinterpret_cast<PDEV_BROADCAST_PORT>(hdr));
			break;
		case DBT_DEVTYP_VOLUME:
			static_cast<T*>(this)->OnVolumeQueryRemoveFailed(
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr));
			break;
		}
	}

	void _OnDeviceRemoveComplete(PDEV_BROADCAST_HDR hdr)
	{
		switch (hdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			static_cast<T*>(this)->OnDeviceInterfaceRemoveComplete(
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(hdr));
			break;
		case DBT_DEVTYP_HANDLE:
			static_cast<T*>(this)->OnDeviceHandleRemoveComplete(
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(hdr));
			break;
		case DBT_DEVTYP_OEM:
			static_cast<T*>(this)->OnOemRemoveComplete(
				reinterpret_cast<PDEV_BROADCAST_OEM>(hdr));
			break;
		case DBT_DEVTYP_PORT:
			static_cast<T*>(this)->OnPortRemoveComplete(
				reinterpret_cast<PDEV_BROADCAST_PORT>(hdr));
			break;
		case DBT_DEVTYP_VOLUME:
			static_cast<T*>(this)->OnVolumeRemoveComplete(
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr));
			break;
		}
	}

	void _OnDeviceRemovePending(PDEV_BROADCAST_HDR hdr)
	{
		switch (hdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			static_cast<T*>(this)->OnDeviceInterfaceRemovePending(
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(hdr));
			break;
		case DBT_DEVTYP_HANDLE:
			static_cast<T*>(this)->OnDeviceHandleRemovePending(
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(hdr));
			break;
		case DBT_DEVTYP_OEM:
			static_cast<T*>(this)->OnOemRemovePending(
				reinterpret_cast<PDEV_BROADCAST_OEM>(hdr));
			break;
		case DBT_DEVTYP_PORT:
			static_cast<T*>(this)->OnPortRemovePending(
				reinterpret_cast<PDEV_BROADCAST_PORT>(hdr));
			break;
		case DBT_DEVTYP_VOLUME:
			static_cast<T*>(this)->OnVolumeRemovePending(
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr));
			break;
		}
	}

	void _OnDeviceTypeSpecific(PDEV_BROADCAST_HDR hdr)
	{
		switch (hdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			static_cast<T*>(this)->OnDeviceInterfaceTypeSpecific(
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(hdr));
			break;
		case DBT_DEVTYP_HANDLE:
			static_cast<T*>(this)->OnDeviceHandleTypeSpecific(
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(hdr));
			break;
		case DBT_DEVTYP_OEM:
			static_cast<T*>(this)->OnOemTypeSpecific(
				reinterpret_cast<PDEV_BROADCAST_OEM>(hdr));
			break;
		case DBT_DEVTYP_PORT:
			static_cast<T*>(this)->OnPortTypeSpecific(
				reinterpret_cast<PDEV_BROADCAST_PORT>(hdr));
			break;
		case DBT_DEVTYP_VOLUME:
			static_cast<T*>(this)->OnVolumeTypeSpecific(
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr));
			break;
		}
	}

	void OnDeviceInterfaceCustomEvent(PDEV_BROADCAST_DEVICEINTERFACE) {}
	void OnDeviceHandleCustomEvent(PDEV_BROADCAST_HANDLE) {}
	void OnOemCustomEvent(PDEV_BROADCAST_OEM) {}
	void OnPortCustomEvent(PDEV_BROADCAST_PORT) {}
	void OnVolumeCustomEvent(PDEV_BROADCAST_VOLUME) {}

	void OnDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE) {}
	void OnDeviceHandleArrival(PDEV_BROADCAST_HANDLE) {}
	void OnOemArrival(PDEV_BROADCAST_OEM) {}
	void OnPortArrival(PDEV_BROADCAST_PORT) {}
	void OnVolumeArrival(PDEV_BROADCAST_VOLUME) {}

	LRESULT OnDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE) { return TRUE; }
	LRESULT OnDeviceHandleQueryRemove(PDEV_BROADCAST_HANDLE) { return TRUE; }
	LRESULT OnOemQueryRemove(PDEV_BROADCAST_OEM) { return TRUE; }
	LRESULT OnPortQueryRemove(PDEV_BROADCAST_PORT) { return TRUE; }
	LRESULT OnVolumeQueryRemove(PDEV_BROADCAST_VOLUME) { return TRUE; }

	void OnDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE) {}
	void OnDeviceHandleQueryRemoveFailed(PDEV_BROADCAST_HANDLE) {}
	void OnOemQueryRemoveFailed(PDEV_BROADCAST_OEM) {}
	void OnPortQueryRemoveFailed(PDEV_BROADCAST_PORT) {}
	void OnVolumeQueryRemoveFailed(PDEV_BROADCAST_VOLUME) {}

	void OnDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE) {}
	void OnDeviceHandleRemoveComplete(PDEV_BROADCAST_HANDLE) {}
	void OnOemRemoveComplete(PDEV_BROADCAST_OEM) {}
	void OnPortRemoveComplete(PDEV_BROADCAST_PORT) {}
	void OnVolumeRemoveComplete(PDEV_BROADCAST_VOLUME) {}

	void OnDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE) {}
	void OnDeviceHandleRemovePending(PDEV_BROADCAST_HANDLE) {}
	void OnOemRemovePending(PDEV_BROADCAST_OEM) {}
	void OnPortRemovePending(PDEV_BROADCAST_PORT) {}
	void OnVolumeRemovePending(PDEV_BROADCAST_VOLUME) {}

	void OnDeviceInterfaceTypeSpecific(PDEV_BROADCAST_DEVICEINTERFACE) {}
	void OnDeviceHandleTypeSpecific(PDEV_BROADCAST_HANDLE) {}
	void OnOemTypeSpecific(PDEV_BROADCAST_OEM) {}
	void OnPortTypeSpecific(PDEV_BROADCAST_PORT) {}
	void OnVolumeTypeSpecific(PDEV_BROADCAST_VOLUME) {}

	void OnUserDefined(_DEV_BROADCAST_USERDEFINED* /*pdbuser*/) {}

};

template <typename T>
class CPowerEventHandler
{
public:

	//
	// Power Event Handler Dispatcher
	//
	LRESULT PowerEventProc(WPARAM wParam, LPARAM lParam);

protected:

	//
	// Battery power is low.
	//
	void OnBatteryLow() {}
	//
	// OEM-defined event occurred.
	//
	void OnOemEvent(DWORD /*dwEventCode*/) {}
	//
	// Power status has changed.
	//
	void OnPowerStatusChange() {}
	//
	// Request for permission to suspend.
	//
	// A DWORD value dwFlags specifies action flags. 
	// If bit 0 is 1, the application can prompt the user for directions 
	// on how to prepare for the suspension; otherwise, the application 
	// must prepare without user interaction. 
	// All other bit values are reserved. 
	//
	// Return TRUE to grant the request to suspend. 
	// To deny the request, return BROADCAST_QUERY_DENY.
	//
	LRESULT OnQuerySuspend(DWORD /*dwFlags*/) { return TRUE; }
	//
	// Suspension request denied.
	//
	void OnQuerySuspendFailed() {}
	//
	// Operation resuming automatically after event.
	//
	void OnResumeAutomatic() {}
	//
	// Operation resuming after critical suspension.
	//
	void OnResumeCritical() {}
	//
	// Operation resuming after suspension.
	//
	void OnResumeSuspend() {}
	//
	// System is suspending operation.
	//
	void OnSuspend() {}
};

//
// Return code for services and Windows applications
// for handling Device Events are different
// Device Event Handler is based on Windows application,
// which will return TRUE or BROADCAST_QUERY_DENY
//
// For services:
//
// If your service handles HARDWAREPROFILECHANGE, 
// return NO_ERROR to grant the request 
// and an error code to deny the request.
//
// CServiceDeviceEventHandler and CServicePowerEventHandler
// shim the differeces of parameters and the return code.
//
template <typename T>
class CServiceDeviceEventHandler : public CDeviceEventHandler<T>
{
public:
	// Forward Device Event to DeviceEventProc
	DWORD ServiceDeviceEventHandler(DWORD dwEventType, LPVOID lpEventData)
	{
		T* pT = static_cast<T*>(this);
		LRESULT res = pT->DeviceEventProc(
			static_cast<WPARAM>(dwEventType), 
			reinterpret_cast<LPARAM>(lpEventData));
		return (TRUE == res) ? NO_ERROR : res;
	}
	// Forward HardwareProfileChange Event to DeviceEventProc
	DWORD ServiceHardwareProfileChangeHandler(DWORD dwEventType, LPVOID lpEventData) 
	{
		T* pT = static_cast<T*>(this);
		LRESULT res = pT->DeviceEventProc(
			static_cast<WPARAM>(dwEventType), 
			reinterpret_cast<LPARAM>(lpEventData));
		return (TRUE == res) ? NO_ERROR : res;
	}
};

template <typename T>
class CServicePowerEventHandler : public CPowerEventHandler<T>
{
public:
	// Forward Power Event to PowerEventProc
	DWORD ServicePowerEventHandler(DWORD dwEventType, LPVOID lpEventData) 
	{
		T* pT = static_cast<T*>(this);
		LRESULT res = pT->PowerEventProc(
			static_cast<WPARAM>(dwEventType), 
			reinterpret_cast<LPARAM>(lpEventData));
		return (TRUE == res) ? NO_ERROR : res;
	}
};


template <typename T> inline LRESULT
CDeviceEventHandler<T>::DeviceEventProc(WPARAM wParam, LPARAM lParam)
{
	T* pT = static_cast<T*>(this);
	switch (wParam) 
	{
	case DBT_CUSTOMEVENT:
		_OnCustomEvent(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEARRIVAL:
		_OnDeviceArrival(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEQUERYREMOVE:
		return _OnDeviceQueryRemove(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICEQUERYREMOVEFAILED:
		_OnDeviceQueryRemoveFailed(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEREMOVECOMPLETE:
		_OnDeviceRemoveComplete(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEREMOVEPENDING:
		_OnDeviceRemovePending(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICETYPESPECIFIC:
		_OnDeviceTypeSpecific(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVNODES_CHANGED:
		XTLASSERT(lParam == 0);
		pT->OnDevNodesChanged();
		return TRUE;
	case DBT_CONFIGCHANGECANCELED:
		XTLASSERT(lParam == 0);
		pT->OnConfigChangeCanceled();
		return TRUE;
	case DBT_CONFIGCHANGED:
		XTLASSERT(lParam == 0);
		pT->OnConfigChanged();
		return TRUE;
	case DBT_QUERYCHANGECONFIG:
		XTLASSERT(lParam == 0);
		return pT->OnQueryChangeConfig();
	case DBT_USERDEFINED:
		pT->OnUserDefined(reinterpret_cast<_DEV_BROADCAST_USERDEFINED*>(lParam));
		return TRUE;
	default:
		XTLASSERT(FALSE && "Unknown Device Event");
		return TRUE; // or BROADCAST_QUERY_DENY 
	}
}

template <typename T> inline LRESULT
CPowerEventHandler<T>::PowerEventProc(WPARAM wParam, LPARAM lParam)
{
	T* pT = static_cast<T*>(this);
	switch (wParam) 
	{
	case PBT_APMBATTERYLOW:
		// Battery power is low.
		pT->OnBatteryLow();
		return TRUE;
	case PBT_APMOEMEVENT:
		// OEM-defined event occurred.
		pT->OnOemEvent(static_cast<DWORD>(lParam));
		return TRUE;
	case PBT_APMPOWERSTATUSCHANGE: 
		// Power status has changed.
		pT->OnPowerStatusChange();
		return TRUE;
	case PBT_APMQUERYSUSPEND:
		// Request for permission to suspend.
		return pT->OnQuerySuspend(static_cast<DWORD>(lParam));
	case PBT_APMQUERYSUSPENDFAILED:
		// Suspension request denied.
		pT->OnQuerySuspendFailed();
		return TRUE;
	case PBT_APMRESUMEAUTOMATIC:
		// Operation resuming automatically after event.
		pT->OnResumeAutomatic();
		return TRUE;
	case PBT_APMRESUMECRITICAL:
		// Operation resuming after critical suspension.
		pT->OnResumeCritical();
		return TRUE;
	case PBT_APMRESUMESUSPEND:
		// Operation resuming after suspension.
		pT->OnResumeSuspend();
		return TRUE;
	case PBT_APMSUSPEND:
		// System is suspending operation.
		pT->OnSuspend();
		return TRUE;
	default:
		XTLASSERT(FALSE && "Unknown Power Event");
		return TRUE;
	}
}

} // namespace XTL