#include "stdafx.h"
#include "pnpevent.h"
#include "xguid.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_PNPEVENT
#include "xdebug.h"

namespace ximeta {

#define BEGIN_ENUM_STRING(x) switch(x) {
#define ENUM_STRING(x) case x: {return TEXT(#x);}
#define ENUM_STRING_DEFAULT(x) default: {return TEXT(x);}
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

LRESULT
CDeviceEventHandler::OnDeviceEvent(WPARAM wParam, LPARAM lParam)
{
	DPInfo(_FT("%s\n"), DBT_String(wParam));

	switch (wParam) {
	case DBT_CUSTOMEVENT:
		return OnCustomEvent(
			reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICEARRIVAL:
		return OnDeviceArrival(
			reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICEQUERYREMOVE:
		return OnDeviceQueryRemove(
			reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICEQUERYREMOVEFAILED:
		return OnDeviceQueryRemoveFailed(
			reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICEREMOVECOMPLETE:
		return OnDeviceRemoveComplete(
			reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICEREMOVEPENDING:
		return OnDeviceRemovePending(
			reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));
	case DBT_DEVICETYPESPECIFIC:
		return OnDeviceTypeSpecific(
			reinterpret_cast<PDEV_BROADCAST_HDR>(lParam));

	case DBT_DEVNODES_CHANGED:
		_ASSERTE(lParam == 0);
		return OnDevNodesChanged();
	case DBT_CONFIGCHANGECANCELED:
		_ASSERTE(lParam == 0);
		return OnConfigChangeCanceled();
	case DBT_CONFIGCHANGED:
		_ASSERTE(lParam == 0);
		return OnConfigChanged();

	case DBT_QUERYCHANGECONFIG:
		_ASSERTE(lParam == 0);
		return OnQueryChangeConfig();

	case DBT_USERDEFINED:
		return OnUserDefined(
			reinterpret_cast<_DEV_BROADCAST_USERDEFINED*>(lParam));
	}
	return TRUE; // or BROADCAST_QUERY_DENY 
}

LRESULT
CDeviceEventHandler::OnConfigChangeCanceled()
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnConfigChanged()
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnDevNodesChanged()
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnQueryChangeConfig()
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnCustomEvent(PDEV_BROADCAST_HDR pdbhdr)
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnDeviceArrival(PDEV_BROADCAST_HDR pdbhdr)
{
	return TRUE;
/*
	switch (pdbhdr->dbch_devicetype) {
	case DBT_DEVTYP_DEVICEINTERFACE:
		{
			PDEV_BROADCAST_DEVICEINTERFACE pdbcc = 
				reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);
			CGuid guid(&pdbcc->dbcc_classguid);
			// DBInfo(TEXT(""));
			pdbcc->dbcc_classguid;
			pdbcc->dbcc_devicetype;
		}
	case DBT_DEVTYP_HANDLE:
		{
			PDEV_BROADCAST_HANDLE pdbch = 
				reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);
		}
	case DBT_DEVTYP_OEM:
		{
			PDEV_BROADCAST_OEM pdbo = 
				reinterpret_cast<PDEV_BROADCAST_OEM>(pdbhdr);
		}
	case DBT_DEVTYP_PORT:
		{
			PDEV_BROADCAST_PORT pdbcp = 
				reinterpret_cast<PDEV_BROADCAST_PORT>(pdbhdr);
		}
	case DBT_DEVTYP_VOLUME:
		{
			PDEV_BROADCAST_VOLUME pdbcv = 
				reinterpret_cast<PDEV_BROADCAST_VOLUME>(pdbhdr);
		}
	}
	return TRUE;
*/
}

LRESULT
CDeviceEventHandler::OnDeviceQueryRemove(PDEV_BROADCAST_HDR pdbhdr)
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR pdbhdr)
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnDeviceRemoveComplete(PDEV_BROADCAST_HDR pdbhdr)
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnDeviceRemovePending(PDEV_BROADCAST_HDR pdbhdr)
{
	return TRUE;
}

LRESULT
CDeviceEventHandler::OnDeviceTypeSpecific(PDEV_BROADCAST_HDR pdbhdr)
{
	return TRUE;
}

LRESULT 
CDeviceEventHandler::OnUserDefined(_DEV_BROADCAST_USERDEFINED* pdbuser)
{
	return TRUE;
}

LRESULT
CPowerEventHandler::OnPowerEvent(WPARAM wParam, LPARAM lParam)
{
	DPInfo(_FT("%s\n"), PBT_String(wParam));

	switch (wParam) {
	case PBT_APMBATTERYLOW:
		// Battery power is low.
		OnBatteryLow();
		return TRUE;
	case PBT_APMOEMEVENT:
		// OEM-defined event occurred.
		OnOemEvent(static_cast<DWORD>(lParam));
		return TRUE;
	case PBT_APMPOWERSTATUSCHANGE: 
		// Power status has changed.
		OnPowerStatusChange();
		return TRUE;
	case PBT_APMQUERYSUSPEND:
		// Request for permission to suspend.
		return OnQuerySuspend(static_cast<DWORD>(lParam));
	case PBT_APMQUERYSUSPENDFAILED:
		// Suspension request denied.
		OnQuerySuspendFailed();
		return TRUE;
	case PBT_APMRESUMEAUTOMATIC:
		// Operation resuming automatically after event.
		OnResumeAutomatic();
		return TRUE;
	case PBT_APMRESUMECRITICAL:
		// Operation resuming after critical suspension.
		OnResumeCritical();
		return TRUE;
	case PBT_APMRESUMESUSPEND:
		// Operation resuming after suspension.
		OnResumeSuspend();
		return TRUE;
	case PBT_APMSUSPEND:
		// System is suspending operation.
		OnSuspend();
		return TRUE;
	}

	return TRUE;
}

void 
CPowerEventHandler::OnBatteryLow()
{
}

void 
CPowerEventHandler::OnOemEvent(DWORD dwEventCode)
{
}

void 
CPowerEventHandler::OnPowerStatusChange()
{
}

//
// Return TRUE to grant the request to suspend. 
// To deny the request, return BROADCAST_QUERY_DENY.
//
LRESULT 
CPowerEventHandler::OnQuerySuspend(DWORD dwFlags)
{
	return TRUE;
}

void 
CPowerEventHandler::OnQuerySuspendFailed()
{
}

void 
CPowerEventHandler::OnResumeAutomatic()
{
}

void 
CPowerEventHandler::OnResumeCritical()
{
}

void 
CPowerEventHandler::OnResumeSuspend()
{
}

void 
CPowerEventHandler::OnSuspend()
{
}

}