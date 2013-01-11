#pragma once

#include "xguid.h"
#include "ndasbind.h"

template<typename T>
class ATL_NO_VTABLE CNdasEventHandler :
	public CMessageMap
{
	BEGIN_MSG_MAP_EX(CNdasEventHandler<T>)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_DEVICE_ENTRY_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_DEVICE_STATUS_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_ENTRY_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_STATUS_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_SURRENDER_ACCESS_REQUEST, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_DISCONNECTED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_RECONNECTING, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_RECONNECTED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_RECONNECTED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_SERVICE_TERMINATING, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_DEVICE_PROPERTY_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_UNITDEVICE_PROPERTY_CHANGED, OnNdasDeviceEvent)
	END_MSG_MAP()

	CNdasEventHandler() {}

	LRESULT OnNdasDeviceEvent(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam);

	VOID OnNdasDevEntryChanged() 
	{ ATLTRACE(_T("Device Entry Changed.\n")); }

	VOID OnNdasDevStatusChanged(DWORD dwSlotNo) 
	{ ATLTRACE(_T("Device Status Changed at slot %d.\n"), dwSlotNo); }

	VOID OnNdasDevPropChanged(DWORD dwSlotNo)
	{ ATLTRACE(_T("Device Property Changed at slot %d.\n"), dwSlotNo); }

	VOID OnNdasUnitDevPropChanged(DWORD dwSlotNo, DWORD dwUnitNo)
	{ ATLTRACE(_T("Unit Device Property Changed at slot %d, unit %d.\n"), dwSlotNo, dwUnitNo); }

	VOID OnNdasLogDevEntryChanged()
	{ ATLTRACE(_T("Logical Device Entry Changed.\n")); }

	VOID OnNdasLogDevStatusChanged(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d) Status Changed.\n"), logDevId); }

	VOID OnNdasLogDevDisconnected(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d) is disconnected.\n"), logDevId); }

	VOID OnNdasLogDevReconnecting(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d) is being reconnecting.\n"), logDevId); }

	VOID OnNdasLogDevReconnected(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d) is reconnected.\n"), logDevId); }

	VOID OnNdasSurrenderAccessRequest(
		DWORD dwSlotNo, 
		DWORD dwUnitNo, 
		UCHAR requestFlags,
		LPCGUID requestHostGuid)
	{
		ATLTRACE(_T("SurrenderAccessRequest to (%d,%d) flags: %d from %s)"),
			dwSlotNo, 
			dwUnitNo, 
			requestFlags, 
			ximeta::CGuid(requestHostGuid).ToString());
	}

	VOID OnNdasServiceTerminating()
	{ ATLTRACE(_T("Service is terminating.\n")); }

};

template <typename T>
inline
LRESULT 
CNdasEventHandler<T>::
OnNdasDeviceEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD dwSlotNo = static_cast<DWORD>(lParam);
	DWORD dwUnitNo = static_cast<DWORD>(wParam);
	NDAS_LOGICALDEVICE_ID logDevId = static_cast<DWORD>(lParam);

	T* pThis = static_cast<T*>(this);

	switch (uMsg) {
	case WM_APP_NDAS_DEVICE_ENTRY_CHANGED:

		pThis->OnNdasDevEntryChanged();
		break;

	case WM_APP_NDAS_DEVICE_STATUS_CHANGED:

		pThis->OnNdasDevStatusChanged(dwSlotNo);
		break;

	case WM_APP_NDAS_DEVICE_PROPERTY_CHANGED:

		pThis->OnNdasDevPropChanged(dwSlotNo);
		break;

	case WM_APP_NDAS_UNITDEVICE_PROPERTY_CHANGED:

		pThis->OnNdasUnitDevPropChanged(dwSlotNo, dwUnitNo);
		break;

	case WM_APP_NDAS_LOGICALDEVICE_ENTRY_CHANGED:

		pThis->OnNdasLogDevEntryChanged();
		break;

	case WM_APP_NDAS_LOGICALDEVICE_STATUS_CHANGED:

		pThis->OnNdasLogDevStatusChanged(logDevId);
		break;

	case WM_APP_NDAS_LOGICALDEVICE_DISCONNECTED:

		pThis->OnNdasLogDevDisconnected(logDevId);
		break;

	case WM_APP_NDAS_LOGICALDEVICE_RECONNECTING:

		pThis->OnNdasLogDevReconnecting(logDevId);
		break;

	case WM_APP_NDAS_LOGICALDEVICE_RECONNECTED:

		pThis->OnNdasLogDevReconnected(logDevId);
		break;

	case WM_APP_NDAS_SURRENDER_ACCESS_REQUEST:

		{
			PNDAS_EVENT_SURRENDER_REQUEST_INFO pInfo = 
				reinterpret_cast<PNDAS_EVENT_SURRENDER_REQUEST_INFO>(lParam);

			pThis->OnNdasSurrenderAccessRequest(
				pInfo->SlotNo,
				pInfo->UnitNo,
				static_cast<UCHAR>(pInfo->RequestFlags),
				&pInfo->RequestHostGuid);

			::GlobalFree(static_cast<HGLOBAL>(pInfo));
		}
		break;

	case WM_APP_NDAS_SERVICE_TERMINATING:

		pThis->OnNdasServiceTerminating();
		break;

	default:
		SetMsgHandled(FALSE);
	}

	return TRUE;
}
