#pragma once
#include <dbt.h>
#include <pbt.h>
#include "xtldef.h"

namespace XTL
{

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

	void OnCustomEvent(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceArrival(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	LRESULT OnDeviceQueryRemove(PDEV_BROADCAST_HDR /*pdbhdr*/) { return TRUE; }
	void OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceRemoveComplete(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceRemovePending(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceTypeSpecific(PDEV_BROADCAST_HDR /*pdbhdr*/) {}

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
		pT->OnCustomEvent(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEARRIVAL:
		pT->OnDeviceArrival(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEQUERYREMOVE:
		return pT->OnDeviceQueryRemove(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICEQUERYREMOVEFAILED:
		pT->OnDeviceQueryRemoveFailed(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEREMOVECOMPLETE:
		pT->OnDeviceRemoveComplete(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICEREMOVEPENDING:
		pT->OnDeviceRemovePending(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
		return TRUE;
	case DBT_DEVICETYPESPECIFIC:
		pT->OnDeviceTypeSpecific(reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
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