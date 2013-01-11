#ifndef _PNPEVENT_PROCESSOR_H_
#define _PNPEVENT_PROCESSOR_H_
#pragma once

//
// Using DebugPrint is invasive.
// Use DebugPrint only if it is supplied by the implementor
// So, the implementor should include "xdbgprn.h" prior to include
// this header to make it effective.
//
#ifdef _X_DBGPRINT_H_
#define HDebugPrint	DebugPrint
#else
#define HDebugPrint __noop
#endif

#include <dbt.h>
#include <pbt.h>

#define BEGIN_ENUM_STRING(x) switch(x) {
#define ENUM_STRING(x) case x: {return _T(#x);}
#define ENUM_STRING_DEFAULT(x) default: {return _T(x);}
#define END_ENUM_STRING() }

static LPCTSTR DBT_String(WPARAM wParam)
{
	BEGIN_ENUM_STRING(wParam)
		ENUM_STRING(DBT_CONFIGCHANGECANCELED)
		ENUM_STRING(DBT_CUSTOMEVENT)
		ENUM_STRING(DBT_DEVICEARRIVAL)
		ENUM_STRING(DBT_DEVICEQUERYREMOVE)
		ENUM_STRING(DBT_DEVICEQUERYREMOVEFAILED)
		ENUM_STRING(DBT_DEVICEREMOVECOMPLETE)
		ENUM_STRING(DBT_DEVICEREMOVEPENDING)
		ENUM_STRING(DBT_DEVICETYPESPECIFIC)
		ENUM_STRING(DBT_DEVNODES_CHANGED)
		ENUM_STRING(DBT_QUERYCHANGECONFIG)
		ENUM_STRING(DBT_USERDEFINED)
		ENUM_STRING_DEFAULT("DBT_???")
	END_ENUM_STRING()
}

static LPCTSTR PBT_String(WPARAM wParam)
{
	BEGIN_ENUM_STRING(wParam)
		ENUM_STRING(PBT_APMBATTERYLOW)
		ENUM_STRING(PBT_APMOEMEVENT)
		ENUM_STRING(PBT_APMPOWERSTATUSCHANGE)
		ENUM_STRING(PBT_APMQUERYSUSPEND)
		ENUM_STRING(PBT_APMQUERYSUSPENDFAILED)
		ENUM_STRING(PBT_APMRESUMEAUTOMATIC)
		ENUM_STRING(PBT_APMRESUMECRITICAL)
		ENUM_STRING(PBT_APMRESUMESUSPEND)
		ENUM_STRING(PBT_APMSUSPEND)
		ENUM_STRING_DEFAULT("PBT_???")
	END_ENUM_STRING()
}

static LPCTSTR DBT_DEVTYP_String(DWORD dbch_devicetype)
{
	BEGIN_ENUM_STRING(dbch_devicetype)
		ENUM_STRING(DBT_DEVTYP_DEVICEINTERFACE)
		ENUM_STRING(DBT_DEVTYP_HANDLE)
		ENUM_STRING(DBT_DEVTYP_OEM)
		ENUM_STRING(DBT_DEVTYP_PORT)
		ENUM_STRING(DBT_DEVTYP_VOLUME)
		ENUM_STRING_DEFAULT("DBT_DEVTYPE_???")
	END_ENUM_STRING()
}

static LPCTSTR DbcvFlagsToString(WORD dbcv_flags)
{
	BEGIN_ENUM_STRING(dbcv_flags)
		ENUM_STRING(DBTF_MEDIA)
		ENUM_STRING(DBTF_NET)
		ENUM_STRING_DEFAULT("DBTF_???")
	END_ENUM_STRING()
}

#undef BEGIN_ENUM_STRING
#undef ENUM_STRING
#undef ENUM_STRING_DEFAULT
#undef END_ENUM_STRING

template <typename T>
class CDeviceEventHandlerT
{
protected:

	void OnConfigChangeCanceled() { }
	void OnConfigChanged() { }
	void OnDevNodesChanged() { }
	LRESULT OnQueryChangeConfig() { return TRUE; }

	void OnCustomEvent(PDEV_BROADCAST_HDR pdbhdr) {}
	void OnDeviceArrival(PDEV_BROADCAST_HDR pdbhdr) {}
	LRESULT OnDeviceQueryRemove(PDEV_BROADCAST_HDR pdbhdr) { return TRUE; }
	void OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR pdbhdr) {}
	void OnDeviceRemoveComplete(PDEV_BROADCAST_HDR pdbhdr) {}
	void OnDeviceRemovePending(PDEV_BROADCAST_HDR pdbhdr) {}
	void OnDeviceTypeSpecific(PDEV_BROADCAST_HDR pdbhdr) {}
	void OnUserDefined(_DEV_BROADCAST_USERDEFINED* pdbuser) {}

public:

	LRESULT OnDeviceEvent(WPARAM wParam, LPARAM lParam);
};

template <typename T>
class CPowerEventHandlerT
{
protected:
	//
	// Battery power is low.
	//
	void OnBatteryLow() {}
	//
	// OEM-defined event occurred.
	//
	void OnOemEvent(DWORD dwEventCode) { UNREFERENCED_PARAMETER(dwEventCode); }
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
	LRESULT OnQuerySuspend(DWORD dwFlags) {UNREFERENCED_PARAMETER(dwFlags);}
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

public:

	//
	// Power Event Handler Dispatcher
	//
	LRESULT OnPowerEvent(WPARAM wParam, LPARAM lParam);
};

template <typename T>
inline
LRESULT
CDeviceEventHandlerT<T>::OnDeviceEvent(WPARAM wParam, LPARAM lParam)
{
	HDebugPrint(1, _T("OnDeviceEvent: %s\n"), DBT_String(wParam));

	T* pThis = reinterpret_cast<T*>(this);

	PDEV_BROADCAST_HDR pbh = 
		reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);

	_DEV_BROADCAST_USERDEFINED* pdbuser = 
		reinterpret_cast<_DEV_BROADCAST_USERDEFINED*>(lParam);

	switch (wParam) {
	case DBT_CUSTOMEVENT:
		return pThis->OnCustomEvent(pbh), TRUE;
	case DBT_DEVICEARRIVAL:
		return pThis->OnDeviceArrival(pbh), TRUE;
	case DBT_DEVICEQUERYREMOVE:
		return pThis->OnDeviceQueryRemove(pbh);
	case DBT_DEVICEQUERYREMOVEFAILED:
		return pThis->OnDeviceQueryRemoveFailed(pbh), TRUE;
	case DBT_DEVICEREMOVECOMPLETE:
		return pThis->OnDeviceRemoveComplete(pbh), TRUE;
	case DBT_DEVICEREMOVEPENDING:
		return pThis->OnDeviceRemovePending(pbh), TRUE;
	case DBT_DEVICETYPESPECIFIC:
		return pThis->OnDeviceTypeSpecific(pbh), TRUE;
	case DBT_DEVNODES_CHANGED:
		_ASSERTE(lParam == 0);
		return pThis->OnDevNodesChanged(), TRUE;
	case DBT_CONFIGCHANGECANCELED:
		_ASSERTE(lParam == 0);
		return pThis->OnConfigChangeCanceled(), TRUE;
	case DBT_CONFIGCHANGED:
		_ASSERTE(lParam == 0);
		return pThis->OnConfigChanged(), TRUE;
	case DBT_QUERYCHANGECONFIG:
		_ASSERTE(lParam == 0);
		return pThis->OnQueryChangeConfig();
	case DBT_USERDEFINED:
		return pThis->OnUserDefined(pdbuser), TRUE;
	}
	return TRUE; // or BROADCAST_QUERY_DENY 
}

template <typename T>
inline
LRESULT
CPowerEventHandlerT<T>::OnPowerEvent(WPARAM wParam, LPARAM lParam)
{
	HDebugPrint(1, _T("%s\n"), PBT_String(wParam));

	T* pThis = reinterpret_cast<T*>(this);

	switch (wParam) {
	case PBT_APMBATTERYLOW:
		// Battery power is low.
		return pThis->OnBatteryLow(), TRUE;
	case PBT_APMOEMEVENT:
		// OEM-defined event occurred.
		return pThis->OnOemEvent(static_cast<DWORD>(lParam)), TRUE;
	case PBT_APMPOWERSTATUSCHANGE: 
		// Power status has changed.
		return pThis->OnPowerStatusChange(), TRUE;
	case PBT_APMQUERYSUSPEND:
		// Request for permission to suspend.
		return pThis->OnQuerySuspend(static_cast<DWORD>(lParam));
	case PBT_APMQUERYSUSPENDFAILED:
		// Suspension request denied.
		return pThis->OnQuerySuspendFailed(), TRUE;
	case PBT_APMRESUMEAUTOMATIC:
		// Operation resuming automatically after event.
		return pThis->OnResumeAutomatic(), TRUE;
	case PBT_APMRESUMECRITICAL:
		// Operation resuming after critical suspension.
		return pThis->OnResumeCritical(), TRUE;
	case PBT_APMRESUMESUSPEND:
		// Operation resuming after suspension.
		return pThis->OnResumeSuspend(), TRUE;
	case PBT_APMSUSPEND:
		// System is suspending operation.
		return pThis->OnSuspend(), TRUE;
	}

	return TRUE;
}

#endif

