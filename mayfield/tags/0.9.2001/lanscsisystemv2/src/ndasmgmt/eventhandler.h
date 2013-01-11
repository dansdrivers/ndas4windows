#pragma once
#include "ndasmgmt.h"

template<typename T>
class ATL_NO_VTABLE CNdasEventHandler :
	public CMessageMap
{
	BEGIN_MSG_MAP_EX(CNdasEventHandler<T>)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_DEVICE_ENTRY_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_DEVICE_STATUS_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_ENTRY_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_STATUS_CHANGED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_DISCONNECTED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_RECONNECTING, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_LOGICALDEVICE_RECONNECTED, OnNdasDeviceEvent)
		MESSAGE_HANDLER_EX(WM_APP_NDAS_SERVICE_TERMINATING, OnNdasDeviceEvent)
	END_MSG_MAP()

	CNdasEventHandler() {}

	LRESULT OnNdasDeviceEvent(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam);

	void OnNdasDevEntryChanged() 
	{ ATLTRACE(_T("Device Entry Changed.\n")); }

	void OnNdasDevStatusChanged(DWORD dwSlotNo) 
	{ ATLTRACE(_T("Device Status Changed at slot %d.\n"), dwSlotNo); }

	void OnNdasLogDevEntryChanged()
	{ ATLTRACE(_T("Logical Device Entry Changed.\n")); }

	void OnNdasLogDevStatusChanged(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d,%d,%d) Status Changed.\n"),
		logDevId.SlotNo, logDevId.TargetId, logDevId.LUN); }

	void OnNdasLogDevDisconnected(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d,%d,%d) is disconnected.\n"),
		logDevId.SlotNo, logDevId.TargetId, logDevId.LUN); }

	void OnNdasLogDevReconnecting(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d,%d,%d) is being reconnecting.\n"),
		logDevId.SlotNo, logDevId.TargetId, logDevId.LUN); }

	void OnNdasLogDevReconnected(const NDAS_LOGICALDEVICE_ID& logDevId)
	{ ATLTRACE(_T("Logical Device (%d,%d,%d) is reconnected.\n"),
		logDevId.SlotNo, logDevId.TargetId, logDevId.LUN); }

	void OnNdasServiceTerminating()
	{ ATLTRACE(_T("Service is terminating.\n")); }

};

template <typename T>
inline
LRESULT 
CNdasEventHandler<T>::
OnNdasDeviceEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD dwSlotNo = static_cast<DWORD>(lParam);
	NDAS_LOGICALDEVICE_ID logDevId = {
		static_cast<DWORD>(lParam),
			static_cast<DWORD>(LOWORD(wParam)),
			static_cast<DWORD>(HIWORD(lParam))
	};

	T* pThis = static_cast<T*>(this);

	switch (uMsg) {
	case WM_APP_NDAS_DEVICE_ENTRY_CHANGED:

		pThis->OnNdasDevEntryChanged();
		break;

	case WM_APP_NDAS_DEVICE_STATUS_CHANGED:

		pThis->OnNdasDevStatusChanged(dwSlotNo);
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

	case WM_APP_NDAS_SERVICE_TERMINATING:

		pThis->OnNdasServiceTerminating();
		break;

	default:
		SetMsgHandled(FALSE);
	}

	return TRUE;
}
